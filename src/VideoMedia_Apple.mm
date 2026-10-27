// D:\workspace\Fanan_web_hub\src\VideoMedia_Apple.mm

#include "VideoMedia_Apple.h"

#if JUCE_MAC

#include "VideoMp4.h"

#include <juce_audio_formats/juce_audio_formats.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

// Manual retain and release, as in every other Obj-C++ file of Sonify's (no ARC in this build).

namespace
{
    //==============================================================================
    void sleepMs (int ms)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (ms));
    }

    double nowSeconds() noexcept
    {
        return juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    bool stopNow (std::atomic<bool>* cancelled) noexcept
    {
        return cancelled != nullptr && cancelled->load();
    }

    NSURL* urlFor (const juce::File& file)
    {
        auto* path = [NSString stringWithUTF8String: file.getFullPathName().toRawUTF8()];
        return path != nil ? [NSURL fileURLWithPath: path] : nil;
    }

    AVURLAsset* openAsset (const juce::File& file)
    {
        auto* url = urlFor (file);

        if (url == nil)
            return nil;

        // No network, and the tracks read as they are asked for.
        auto* options = @{ AVURLAssetPreferPreciseDurationAndTimingKey : @YES };
        return [[AVURLAsset alloc] initWithURL: url options: options];
    }

    AVAssetTrack* firstTrack (AVURLAsset* asset, AVMediaType kind)
    {
        auto* tracks = [asset tracksWithMediaType: kind];
        return tracks.count > 0 ? [tracks objectAtIndex: 0] : nil;
    }

    /** The picture's size as it is shown: the track's own size with its transform applied (a
        video recorded on a phone carries its turn in that transform, not in its pixels). */
    juce::Point<int> shownSize (AVAssetTrack* video)
    {
        if (video == nil)
            return {};

        const auto size = CGSizeApplyAffineTransform (video.naturalSize, video.preferredTransform);
        return { (int) std::lround (std::abs (size.width)), (int) std::lround (std::abs (size.height)) };
    }

    /** A frame of BGRA pixels as a juce Image. CVPixelBuffer's 32BGRA is byte for byte what
        juce's ARGB images hold on a little-endian machine, so each row is one copy. */
    juce::Image imageFrom (CVPixelBufferRef buffer, int wantedWidth, int wantedHeight)
    {
        if (buffer == nullptr)
            return {};

        if (CVPixelBufferLockBaseAddress (buffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
            return {};

        const auto width = (int) CVPixelBufferGetWidth (buffer);
        const auto height = (int) CVPixelBufferGetHeight (buffer);
        const auto stride = (int) CVPixelBufferGetBytesPerRow (buffer);
        const auto* bytes = static_cast<const juce::uint8*> (CVPixelBufferGetBaseAddress (buffer));

        juce::Image image;

        if (bytes != nullptr && width > 0 && height > 0)
        {
            image = juce::Image (juce::Image::ARGB, width, height, false, juce::SoftwareImageType());
            juce::Image::BitmapData dest (image, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < height; ++y)
                std::memcpy (dest.getLinePointer (y), bytes + (size_t) y * (size_t) stride,
                             (size_t) juce::jmin (stride, dest.lineStride));
        }

        CVPixelBufferUnlockBaseAddress (buffer, kCVPixelBufferLock_ReadOnly);

        if (! image.isValid())
            return {};

        if (wantedWidth > 0 && wantedHeight > 0 && (wantedWidth != width || wantedHeight != height))
            return image.rescaled (wantedWidth, wantedHeight, juce::Graphics::highResamplingQuality);

        return image;
    }

    juce::Image imageFrom (CGImageRef cg, int wantedWidth, int wantedHeight)
    {
        if (cg == nullptr)
            return {};

        const auto width = wantedWidth > 0 ? wantedWidth : (int) CGImageGetWidth (cg);
        const auto height = wantedHeight > 0 ? wantedHeight : (int) CGImageGetHeight (cg);

        if (width <= 0 || height <= 0)
            return {};

        juce::Image image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());

        {
            juce::Image::BitmapData dest (image, juce::Image::BitmapData::writeOnly);
            auto* space = CGColorSpaceCreateDeviceRGB();

            if (space == nullptr)
                return {};

            auto* context = CGBitmapContextCreate (dest.data, (size_t) width, (size_t) height, 8,
                                                   (size_t) dest.lineStride, space,
                                                   kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
            CGColorSpaceRelease (space);

            if (context == nullptr)
                return {};

            CGContextDrawImage (context, CGRectMake (0, 0, width, height), cg);
            CGContextRelease (context);
        }

        return image;
    }
}

//==============================================================================
juce::String AppleVideo::problem()
{
    return {};   // AVFoundation is the system's: there is nothing to find or load.
}

std::optional<VideoMedia::Info> AppleVideo::probe (const juce::File& video, juce::String& error)
{
    if (! video.existsAsFile())
    {
        error = video.getFullPathName() + " isn't there.";
        return std::nullopt;
    }

    auto* asset = openAsset (video);

    if (asset == nil)
    {
        error = "Couldn't open " + video.getFileName() + ".";
        return std::nullopt;
    }

    VideoMedia::Info info;
    auto* picture = firstTrack (asset, AVMediaTypeVideo);
    auto* sound = firstTrack (asset, AVMediaTypeAudio);

    if (picture == nil && sound == nil)
    {
        [asset release];
        error = "There is no picture or sound in " + video.getFileName() + ".";
        return std::nullopt;
    }

    const auto duration = CMTimeGetSeconds (asset.duration);
    info.seconds = std::isfinite (duration) && duration > 0.0 ? duration : 0.0;

    if (picture != nil)
    {
        const auto size = shownSize (picture);
        info.width = size.x;
        info.height = size.y;
        const auto fps = picture.nominalFrameRate;
        info.fps = std::isfinite (fps) && fps > 0.0f ? (double) fps : 0.0;

        if (info.seconds <= 0.0)
        {
            const auto length = CMTimeGetSeconds (picture.timeRange.duration);

            if (std::isfinite (length) && length > 0.0)
                info.seconds = length;
        }
    }

    if (sound != nil)
    {
        info.hasSound = true;

        // The track's own rate, from its format description.
        for (id description in sound.formatDescriptions)
        {
            auto format = (CMAudioFormatDescriptionRef) description;

            if (const auto* basic = CMAudioFormatDescriptionGetStreamBasicDescription (format))
                if (basic->mSampleRate > 0.0)
                {
                    info.soundRate = basic->mSampleRate;
                    break;
                }
        }
    }

    [asset release];

    if (info.width <= 0 && ! info.hasSound)
    {
        error = "There is no picture in " + video.getFileName() + ".";
        return std::nullopt;
    }

    return info;
}

//==============================================================================
juce::Result AppleVideo::readSound (const juce::File& video, const juce::File& wav,
                                   const std::function<void (double)>& progress,
                                   std::atomic<bool>* cancelled, double* reached)
{
    auto* asset = openAsset (video);

    if (asset == nil)
        return juce::Result::fail ("Couldn't open " + video.getFileName() + ".");

    auto* track = firstTrack (asset, AVMediaTypeAudio);

    if (track == nil)
    {
        [asset release];
        return juce::Result::fail ("empty");   // VideoMedia's word for "it has no sound after all"
    }

    const auto channels = juce::jlimit (1, 2, [&]
    {
        for (id description in track.formatDescriptions)
            if (const auto* basic = CMAudioFormatDescriptionGetStreamBasicDescription ((CMAudioFormatDescriptionRef) description))
                return (int) basic->mChannelsPerFrame;

        return 2;
    }());

    double rate = 48000.0;

    for (id description in track.formatDescriptions)
        if (const auto* basic = CMAudioFormatDescriptionGetStreamBasicDescription ((CMAudioFormatDescriptionRef) description))
            if (basic->mSampleRate > 0.0)
            {
                rate = basic->mSampleRate;
                break;
            }

    NSError* problem = nil;
    auto* reader = [[AVAssetReader alloc] initWithAsset: asset error: &problem];

    if (reader == nil)
    {
        [asset release];
        return juce::Result::fail ("Couldn't read the sound of " + video.getFileName() + ".");
    }

    // Float 32, each channel its own buffer: juce writes exactly that.
    auto* settings = @{
        AVFormatIDKey               : @(kAudioFormatLinearPCM),
        AVLinearPCMBitDepthKey      : @32,
        AVLinearPCMIsFloatKey       : @YES,
        AVLinearPCMIsBigEndianKey   : @NO,
        AVLinearPCMIsNonInterleaved : @YES,
        AVSampleRateKey             : @(rate),
        AVNumberOfChannelsKey       : @(channels)
    };

    auto* output = [[AVAssetReaderTrackOutput alloc] initWithTrack: track outputSettings: settings];
    output.alwaysCopiesSampleData = NO;

    if (! [reader canAddOutput: output])
    {
        [output release];
        [reader release];
        [asset release];
        return juce::Result::fail ("Couldn't read the sound of " + video.getFileName() + ".");
    }

    [reader addOutput: output];

    if (! [reader startReading])
    {
        [output release];
        [reader release];
        [asset release];
        return juce::Result::fail ("Couldn't read the sound of " + video.getFileName() + ".");
    }

    wav.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = wav.createOutputStream();

    if (stream == nullptr)
    {
        [reader cancelReading];
        [output release];
        [reader release];
        [asset release];
        return juce::Result::fail ("Couldn't write " + wav.getFullPathName());
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                            .withSampleRate (rate)
                                                            .withNumChannels (channels)
                                                            .withBitsPerSample (32)
                                                            .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)));

    if (writer == nullptr)
    {
        [reader cancelReading];
        [output release];
        [reader release];
        [asset release];
        return juce::Result::fail ("Couldn't write " + wav.getFullPathName());
    }

    const auto length = juce::jmax (0.0, CMTimeGetSeconds (asset.duration));
    juce::int64 written = 0;
    juce::Result result = juce::Result::ok();

    for (;;)
    {
        if (stopNow (cancelled))
        {
            result = juce::Result::fail ("Cancelled");
            break;
        }

        auto sample = [output copyNextSampleBuffer];

        if (sample == nullptr)
            break;

        AudioBufferList list {};
        CMBlockBufferRef block = nullptr;

        const auto got = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer (
            sample, nullptr, &list, sizeof (list), nullptr, nullptr,
            kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &block);

        if (got == noErr && list.mNumberBuffers > 0)
        {
            const auto frames = (int) (list.mBuffers[0].mDataByteSize / sizeof (float));
            const float* channelData[2] = { nullptr, nullptr };

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto which = (UInt32) channel < list.mNumberBuffers ? (UInt32) channel : 0;
                channelData[channel] = static_cast<const float*> (list.mBuffers[which].mData);
            }

            if (frames > 0 && channelData[0] != nullptr
                && ! writer->writeFromFloatArrays (channelData, channels, frames))
                result = juce::Result::fail ("Couldn't write " + wav.getFullPathName());
            else
                written += frames;
        }

        if (block != nullptr)
            CFRelease (block);

        CFRelease (sample);

        if (result.failed())
            break;

        if (progress && length > 0.0)
            progress (juce::jlimit (0.0, 1.0, (double) written / rate / length));
    }

    const auto status = reader.status;
    [reader cancelReading];
    writer.reset();

    [output release];
    [reader release];
    [asset release];

    if (result.failed())
    {
        wav.deleteFile();
        return result;
    }

    if (status == AVAssetReaderStatusFailed || written <= 0)
    {
        wav.deleteFile();
        return juce::Result::fail (written <= 0 ? juce::String ("empty")
                                                : "Couldn't read the sound of " + video.getFileName() + ".");
    }

    if (reached != nullptr)
        *reached = juce::jmax (*reached, (double) written / rate);

    return juce::Result::ok();
}

//==============================================================================
juce::Result AppleVideo::readThumbnails (const juce::File& video, int cellWidth, int cellHeight, double every,
                                        int most, std::vector<juce::Image>& into,
                                        const std::function<void (double)>& progress,
                                        std::atomic<bool>* cancelled, double* reached)
{
    auto* asset = openAsset (video);

    if (asset == nil)
        return juce::Result::fail ("Couldn't open " + video.getFileName() + ".");

    if (firstTrack (asset, AVMediaTypeVideo) == nil)
    {
        [asset release];
        return juce::Result::fail ("There is no picture in " + video.getFileName() + ".");
    }

    auto* generator = [[AVAssetImageGenerator alloc] initWithAsset: asset];
    generator.appliesPreferredTrackTransform = YES;              // a turned video comes out upright
    generator.maximumSize = CGSizeMake (cellWidth * 2, cellHeight * 2);
    generator.apertureMode = AVAssetImageGeneratorApertureModeCleanAperture;

    // The pictures are small and only have to be near the moment they stand for: letting the
    // generator take the nearest frame it already has, rather than the exact one, is many times
    // quicker over a long video.
    const auto slack = CMTimeMakeWithSeconds (juce::jmax (0.05, every * 0.5), 600);
    generator.requestedTimeToleranceBefore = slack;
    generator.requestedTimeToleranceAfter = slack;

    auto length = CMTimeGetSeconds (asset.duration);

    if (! std::isfinite (length) || length <= 0.0)
        length = 0.0;

    const auto wanted = length > 0.0 ? juce::jlimit (1, most, (int) std::floor (length / juce::jmax (0.01, every)) + 1)
                                     : most;

    juce::Result result = juce::Result::ok();

    for (int i = 0; i < wanted; ++i)
    {
        if (stopNow (cancelled))
        {
            result = juce::Result::fail ("Cancelled");
            break;
        }

        const auto at = i * every;

        if (length > 0.0 && at > length)
            break;

        NSError* problem = nil;
        CMTime actual {};
        auto* cg = [generator copyCGImageAtTime: CMTimeMakeWithSeconds (at, 600)
                                     actualTime: &actual
                                          error: &problem];

        if (cg == nullptr)
        {
            // The end of a video often refuses the very last moment: what came before it is enough.
            if (! into.empty())
                break;

            result = juce::Result::fail ("No pictures came out of " + video.getFileName() + ".");
            break;
        }

        auto image = imageFrom (cg, cellWidth, cellHeight);
        CGImageRelease (cg);

        if (! image.isValid())
            break;

        into.push_back (std::move (image));

        if (reached != nullptr)
        {
            const auto was = CMTimeGetSeconds (actual);
            *reached = juce::jmax (*reached, std::isfinite (was) ? was : at);
        }

        if (progress && wanted > 0)
            progress (juce::jlimit (0.0, 1.0, (double) (i + 1) / wanted));
    }

    [generator release];
    [asset release];

    if (result.failed())
        return result;

    if (into.empty())
        return juce::Result::fail ("No pictures came out of " + video.getFileName() + ".");

    return juce::Result::ok();
}

//==============================================================================
juce::Result AppleVideo::transcodePicture (const juce::File& video, const juce::File& target,
                                          const std::function<void (double)>& progress, std::atomic<bool>* cancelled)
{
    auto* asset = openAsset (video);

    if (asset == nil)
        return juce::Result::fail ("Couldn't open " + video.getFileName() + ".");

    auto* track = firstTrack (asset, AVMediaTypeVideo);

    if (track == nil)
    {
        [asset release];
        return juce::Result::fail ("There is no picture in " + video.getFileName() + ".");
    }

    const auto size = shownSize (track);
    target.deleteFile();

    NSError* problem = nil;
    auto* reader = [[AVAssetReader alloc] initWithAsset: asset error: &problem];
    auto* writer = [[AVAssetWriter alloc] initWithURL: urlFor (target)
                                             fileType: AVFileTypeMPEG4
                                                error: &problem];

    const auto giveUp = [&] (const juce::String& why)
    {
        if (reader != nil) { [reader cancelReading]; [reader release]; }
        if (writer != nil) { [writer cancelWriting]; [writer release]; }
        [asset release];
        target.deleteFile();
        return juce::Result::fail (why);
    };

    if (reader == nil || writer == nil)
        return giveUp ("Couldn't write " + target.getFileName() + ".");

    auto* readSettings = @{ (id) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
    auto* output = [[AVAssetReaderTrackOutput alloc] initWithTrack: track outputSettings: readSettings];
    output.alwaysCopiesSampleData = NO;

    auto* writeSettings = @{
        AVVideoCodecKey  : AVVideoCodecTypeH264,
        AVVideoWidthKey  : @(size.x),
        AVVideoHeightKey : @(size.y)
    };

    auto* input = [[AVAssetWriterInput alloc] initWithMediaType: AVMediaTypeVideo
                                                outputSettings: writeSettings];
    input.expectsMediaDataInRealTime = NO;

    // The picture comes out upright: the turn is applied as it is drawn, not carried on.
    input.transform = CGAffineTransformIdentity;

    if (! [reader canAddOutput: output] || ! [writer canAddInput: input])
    {
        [output release];
        [input release];
        return giveUp ("Couldn't write " + target.getFileName() + ".");
    }

    [reader addOutput: output];
    [writer addInput: input];

    if (! [reader startReading] || ! [writer startWriting])
    {
        [output release];
        [input release];
        return giveUp ("Couldn't write " + target.getFileName() + ".");
    }

    [writer startSessionAtSourceTime: kCMTimeZero];

    auto* adaptor = [[AVAssetWriterInputPixelBufferAdaptor alloc]
                        initWithAssetWriterInput: input
                     sourcePixelBufferAttributes: @{ (id) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) }];

    const auto length = juce::jmax (0.0, CMTimeGetSeconds (asset.duration));
    juce::Result result = juce::Result::ok();

    for (;;)
    {
        if (stopNow (cancelled))
        {
            result = juce::Result::fail ("Cancelled");
            break;
        }

        if (! input.isReadyForMoreMediaData)
        {
            sleepMs (4);
            continue;
        }

        auto sample = [output copyNextSampleBuffer];

        if (sample == nullptr)
            break;

        auto buffer = CMSampleBufferGetImageBuffer (sample);
        const auto when = CMSampleBufferGetPresentationTimeStamp (sample);

        if (buffer != nullptr && ! [adaptor appendPixelBuffer: buffer withPresentationTime: when])
            result = juce::Result::fail ("Couldn't write " + target.getFileName() + ".");

        if (progress && length > 0.0)
        {
            const auto at = CMTimeGetSeconds (when);

            if (std::isfinite (at))
                progress (juce::jlimit (0.0, 1.0, at / length));
        }

        CFRelease (sample);

        if (result.failed())
            break;
    }

    [input markAsFinished];

    if (result.wasOk())
    {
        __block bool finished = false;
        [writer finishWritingWithCompletionHandler: ^{ finished = true; }];

        const auto until = nowSeconds() + 600.0;

        while (! finished && nowSeconds() < until)
            sleepMs (10);

        if (writer.status != AVAssetWriterStatusCompleted)
            result = juce::Result::fail ("Couldn't write " + target.getFileName() + ".");
    }

    [adaptor release];
    [output release];
    [input release];
    [reader cancelReading];
    [reader release];
    [writer release];
    [asset release];

    if (result.failed())
    {
        target.deleteFile();
        return result;
    }

    return juce::Result::ok();
}

//==============================================================================
//  The Player: the picture following the song.
//
//  AVPlayer plays at a rate and seeks to a moment exactly, so none of the chasing VLC needs is
//  wanted here: follow() sets the rate once, seeks when the picture has drifted, and takes
//  whatever frame the video output has for the moment it should be showing.
//==============================================================================
struct VideoMedia::Player::Frames
{
    std::mutex lock;
    juce::Image newest;          // waiting to be taken (none: nothing new)
    bool fresh = false;

    AVURLAsset* asset = nil;
    AVPlayerItem* item = nil;
    AVPlayer* player = nil;
    AVPlayerItemVideoOutput* output = nil;

    ~Frames()
    {
        if (player != nil)
        {
            [player pause];
            [player release];
        }

        if (output != nil)   [output release];
        if (item != nil)     [item release];
        if (asset != nil)    [asset release];
    }

    void keep (juce::Image image)
    {
        const std::lock_guard<std::mutex> hold (lock);
        newest = std::move (image);
        fresh = true;
    }

    bool take (juce::Image& into)
    {
        const std::lock_guard<std::mutex> hold (lock);

        if (! fresh)
            return false;

        into = newest;           // the image is shared, never written again: another thread may draw it
        newest = juce::Image();
        fresh = false;
        return true;
    }
};

// Nothing grabs stills on a thread of its own here (AVPlayer gives the frame where it is asked
// to be), but Player holds one, so it has to be a class the destructor can see.
class VideoMedia::Player::Grabber {};

VideoMedia::Player::Player() = default;

VideoMedia::Player::~Player()
{
    close();
}

bool VideoMedia::Player::open (const juce::File& video, const Info& info, juce::Point<int> biggestScreen)
{
    close();

    auto* asset = openAsset (video);

    if (asset == nil)
        return false;

    auto* track = firstTrack (asset, AVMediaTypeVideo);

    if (track == nil)
    {
        [asset release];
        return false;
    }

    auto size = shownSize (track);

    if (info.width > 0 && info.height > 0)
        size = { info.width, info.height };

    if (size.x <= 0 || size.y <= 0)
    {
        [asset release];
        return false;
    }

    // No bigger than the screen it can be shown on (HD at least), nor than 4K.
    const auto mostWide = juce::jlimit (1920, 3840, biggestScreen.x > 0 ? biggestScreen.x : 1920);
    const auto mostHigh = juce::jlimit (1080, 2160, biggestScreen.y > 0 ? biggestScreen.y : 1080);
    const auto shrink = juce::jmin (1.0, (double) mostWide / size.x, (double) mostHigh / size.y);

    outWidth = juce::jmax (2, (int) std::lround (size.x * shrink / 2.0) * 2);
    outHeight = juce::jmax (2, (int) std::lround (size.y * shrink / 2.0) * 2);

    fps = info.fps > 0.0 ? info.fps : (track.nominalFrameRate > 0.0f ? (double) track.nominalFrameRate : 25.0);
    length = info.seconds > 0.0 ? info.seconds : juce::jmax (0.0, CMTimeGetSeconds (asset.duration));

    auto held = std::make_shared<Frames>();
    held->asset = asset;
    held->item = [[AVPlayerItem alloc] initWithAsset: asset];

    auto* attributes = @{
        (id) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
        (id) kCVPixelBufferWidthKey           : @(outWidth),
        (id) kCVPixelBufferHeightKey          : @(outHeight)
    };

    held->output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes: attributes];
    [held->item addOutput: held->output];

    held->player = [[AVPlayer alloc] initWithPlayerItem: held->item];
    held->player.muted = YES;                      // the sound plays in Sonify, never here
    held->player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
    held->player.automaticallyWaitsToMinimizeStalling = NO;
    held->player.rate = 0.0f;

    frames = std::move (held);
    file = video;

    moving = false;
    vlcRate = 1.0;
    wantedRate = 1.0;
    lastDrift = 0.0;
    restarts = 0;
    shownStill = -1.0;
    anchorSeconds = 0.0;
    anchorAt = -1.0;
    return true;
}

void VideoMedia::Player::close()
{
    frames.reset();
    grabber.reset();
    media = nullptr;
    player = nullptr;
    file = juce::File();
    outWidth = outHeight = 0;
    moving = false;
    shownStill = -1.0;
}

void VideoMedia::Player::blank()
{
    if (frames == nullptr)
        return;

    if (frames->player != nil)
        frames->player.rate = 0.0f;

    moving = false;
    shownStill = -1.0;
    frames->keep (juce::Image());
}

void VideoMedia::Player::setVlcRate (double rate)
{
    if (frames == nullptr || frames->player == nil)
        return;

    vlcRate = rate;
    frames->player.rate = (float) rate;
}

double VideoMedia::Player::estimate (double) const
{
    if (frames == nullptr || frames->player == nil)
        return 0.0;

    const auto at = CMTimeGetSeconds (frames->player.currentTime);
    return std::isfinite (at) ? at : 0.0;
}

void VideoMedia::Player::startMoving (double seconds, double rate, double now)
{
    moving = true;
    movedAt = now;
    wantedRate = rate;
    jumpedTo = seconds;
    jumpedAt = now;
    setVlcRate (rate);
}

void VideoMedia::Player::stopMoving()
{
    moving = false;
    setVlcRate (0.0);
}

void VideoMedia::Player::restartVlc (double)
{
    // Nothing to restart: AVPlayer doesn't wander off the way VLC can.
}

void VideoMedia::Player::follow (double seconds, bool shouldMove, double rate)
{
    if (frames == nullptr || frames->player == nil)
        return;

    const auto now = nowSeconds();
    const auto wanted = length > 0.0 ? juce::jlimit (0.0, length, seconds) : juce::jmax (0.0, seconds);
    const auto safeRate = juce::jlimit (0.05, 8.0, rate > 0.0 ? rate : 1.0);

    // Where it is now, and how far that is from where it should be.
    const auto at = estimate (now);
    lastDrift = at - wanted;

    if (shouldMove)
    {
        if (! moving)
            startMoving (wanted, safeRate, now);
        else if (std::abs (safeRate - vlcRate) > 0.001)
            setVlcRate (safeRate);

        // A quarter of a second out: put it where it belongs. Smaller drift is left alone - a
        // seek costs a decode, and the eye doesn't see a frame or two at this size.
        if (std::abs (lastDrift) > 0.25)
        {
            const auto to = CMTimeMakeWithSeconds (wanted, 600);
            const auto slack = CMTimeMakeWithSeconds (juce::jmax (1.0 / juce::jmax (1.0, fps), 0.02), 600);
            [frames->player seekToTime: to toleranceBefore: slack toleranceAfter: slack];
            jumpedTo = wanted;
            jumpedAt = now;
        }

        shownStill = -1.0;
    }
    else
    {
        if (moving)
            stopMoving();

        // Standing still: the frame at that moment, exactly, and only when it has changed.
        if (shownStill < 0.0 || std::abs (shownStill - wanted) > 0.001)
        {
            const auto to = CMTimeMakeWithSeconds (wanted, 600);
            [frames->player seekToTime: to toleranceBefore: kCMTimeZero toleranceAfter: kCMTimeZero];
            shownStill = wanted;
        }
    }

    // Whatever picture the output has for where it should be.
    auto when = [frames->output itemTimeForHostTime: CACurrentMediaTime()];

    if (! CMTIME_IS_VALID (when))
        when = frames->player.currentTime;

    if ([frames->output hasNewPixelBufferForItemTime: when])
    {
        CMTime actual {};
        auto buffer = [frames->output copyPixelBufferForItemTime: when itemTimeForDisplay: &actual];

        if (buffer != nullptr)
        {
            auto image = imageFrom (buffer, outWidth, outHeight);
            CVPixelBufferRelease (buffer);

            if (image.isValid())
            {
                frames->keep (std::move (image));

                if (told)
                    told();
            }
        }
    }
}

bool VideoMedia::Player::takeFrame (juce::Image& image)
{
    if (frames == nullptr)
        return false;

    return frames->take (image);
}

void VideoMedia::Player::setOnFrame (std::function<void()> callback)
{
    told = std::move (callback);
}

#endif
