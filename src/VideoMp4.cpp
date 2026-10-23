// D:\workspace\Fanan_web_hub\src\VideoMp4.cpp

#include "VideoMp4.h"
#include "VideoMedia.h"
#include "Mp3Encoder.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    using Mp4::fourcc;

    juce::uint16 be16 (const juce::uint8* p) noexcept { return static_cast<juce::uint16> ((p[0] << 8) | p[1]); }
    juce::uint32 be32 (const juce::uint8* p) noexcept
    {
        return (static_cast<juce::uint32> (p[0]) << 24) | (static_cast<juce::uint32> (p[1]) << 16)
             | (static_cast<juce::uint32> (p[2]) << 8) | static_cast<juce::uint32> (p[3]);
    }
    juce::uint64 be64 (const juce::uint8* p) noexcept { return (static_cast<juce::uint64> (be32 (p)) << 32) | be32 (p + 4); }

    //==============================================================================
    /** A box in memory: where its contents start and end. */
    struct Box
    {
        juce::uint32 type = 0;
        size_t begin = 0, end = 0;      // its contents
        size_t start = 0;               // its header
    };

    /** The boxes one after another in data[begin, end). */
    std::vector<Box> boxesIn (const juce::uint8* data, size_t begin, size_t end)
    {
        std::vector<Box> boxes;
        auto position = begin;

        while (end > position && end - position >= 8)
        {
            juce::uint64 size = be32 (data + position);
            const auto type = be32 (data + position + 4);
            size_t header = 8;

            if (size == 1)
            {
                if (end - position < 16)
                    break;

                size = be64 (data + position + 8);
                header = 16;
            }
            else if (size == 0)
            {
                size = end - position;
            }

            if (size < header || size > end - position)
                break;

            boxes.push_back ({ type, position + header, position + static_cast<size_t> (size), position });
            position += static_cast<size_t> (size);
        }

        return boxes;
    }

    const Box* findBox (const std::vector<Box>& boxes, juce::uint32 type) noexcept
    {
        for (const auto& box : boxes)
            if (box.type == type)
                return &box;

        return nullptr;
    }

    /** A box's children, by a path of types ("mdia", "minf"...). */
    std::optional<Box> descend (const juce::uint8* data, const Box& from, std::initializer_list<juce::uint32> path)
    {
        auto box = from;

        for (const auto type : path)
        {
            const auto inside = boxesIn (data, box.begin, box.end);
            const auto* next = findBox (inside, type);

            if (next == nullptr)
                return std::nullopt;

            box = *next;
        }

        return box;
    }

    //==============================================================================
    /** An mp4a's object type: its esds, wherever it is in the sample entry (a QuickTime one keeps
        it in a 'wave' box). */
    int objectTypeOf (const juce::uint8* entry, size_t size)
    {
        for (size_t i = 4; i + 4 <= size; ++i)
        {
            if (std::memcmp (entry + i, "esds", 4) != 0)
                continue;

            // The descriptors after the box's version and flags: ES (3), then DecoderConfig (4).
            auto p = i + 8;

            const auto skipLength = [&] (size_t& at)
            {
                for (int n = 0; n < 4 && at < size; ++n)
                    if ((entry[at++] & 0x80) == 0)
                        break;
            };

            if (p >= size || entry[p] != 0x03)
                return 0;

            ++p;
            skipLength (p);

            if (p + 3 > size)
                return 0;

            const auto flags = entry[p + 2];
            p += 3;

            if (flags & 0x80) p += 2;                                   // depends on an ES
            if (flags & 0x40) { if (p < size) p += 1u + entry[p]; }     // a URL
            if (flags & 0x20) p += 2;                                   // OCR ES

            if (p >= size || entry[p] != 0x04)
                return 0;

            ++p;
            skipLength (p);
            return p < size ? entry[p] : 0;
        }

        return 0;
    }

    //==============================================================================
    bool readTables (const juce::uint8* data, const Box& stbl, Mp4::Track& track)
    {
        const auto boxes = boxesIn (data, stbl.begin, stbl.end);
        const auto* stsz = findBox (boxes, fourcc ("stsz"));
        const auto* stz2 = findBox (boxes, fourcc ("stz2"));
        const auto* stsc = findBox (boxes, fourcc ("stsc"));
        const auto* stco = findBox (boxes, fourcc ("stco"));
        const auto* co64 = findBox (boxes, fourcc ("co64"));
        const auto* stts = findBox (boxes, fourcc ("stts"));
        const auto* ctts = findBox (boxes, fourcc ("ctts"));
        const auto* stss = findBox (boxes, fourcc ("stss"));

        if ((stsz == nullptr && stz2 == nullptr) || stsc == nullptr || (stco == nullptr && co64 == nullptr) || stts == nullptr)
            return false;

        const auto fits = [] (const Box& box, size_t from, juce::uint64 count, size_t each)
        {
            return box.end >= box.begin + from && (box.end - box.begin - from) / each >= count;
        };

        // Sizes
        std::vector<juce::uint32> sizes;

        if (stsz != nullptr)
        {
            if (stsz->end - stsz->begin < 12)
                return false;

            const auto constant = be32 (data + stsz->begin + 4);
            const auto count = be32 (data + stsz->begin + 8);

            if (count > 50'000'000u)
                return false;

            if (constant != 0)
            {
                sizes.assign (count, constant);
            }
            else
            {
                if (! fits (*stsz, 12, count, 4))
                    return false;

                sizes.resize (count);

                for (juce::uint32 i = 0; i < count; ++i)
                    sizes[i] = be32 (data + stsz->begin + 12 + 4 * i);
            }
        }
        else
        {
            if (stz2->end - stz2->begin < 12)
                return false;

            const auto field = data[stz2->begin + 7];
            const auto count = be32 (data + stz2->begin + 8);

            if ((field != 4 && field != 8 && field != 16) || count > 50'000'000u
                || (stz2->end - stz2->begin - 12) * 8 / field < count)
                return false;

            sizes.resize (count);

            for (juce::uint32 i = 0; i < count; ++i)
            {
                const auto* p = data + stz2->begin + 12;

                if (field == 16)     sizes[i] = be16 (p + 2 * i);
                else if (field == 8) sizes[i] = p[i];
                else                 sizes[i] = (i & 1) ? (p[i / 2] & 0x0f) : (p[i / 2] >> 4);
            }
        }

        // Chunk offsets
        std::vector<juce::int64> chunks;
        const auto* offsets = co64 != nullptr ? co64 : stco;
        const auto entry = co64 != nullptr ? 8u : 4u;

        if (offsets->end - offsets->begin < 8)
            return false;

        const auto chunkCount = be32 (data + offsets->begin + 4);

        if (! fits (*offsets, 8, chunkCount, entry))
            return false;

        chunks.resize (chunkCount);

        for (juce::uint32 i = 0; i < chunkCount; ++i)
            chunks[i] = co64 != nullptr ? static_cast<juce::int64> (be64 (data + offsets->begin + 8 + 8 * i))
                                        : static_cast<juce::int64> (be32 (data + offsets->begin + 8 + 4 * i));

        // Samples to chunks
        struct Run { juce::uint32 firstChunk, perChunk; };
        std::vector<Run> runs;

        if (stsc->end - stsc->begin < 8)
            return false;

        const auto runCount = be32 (data + stsc->begin + 4);

        if (! fits (*stsc, 8, runCount, 12))
            return false;

        for (juce::uint32 i = 0; i < runCount; ++i)
            runs.push_back ({ be32 (data + stsc->begin + 8 + 12 * i), be32 (data + stsc->begin + 12 + 12 * i) });

        auto& samples = track.samples;
        samples.assign (sizes.size(), {});
        size_t s = 0;

        for (size_t r = 0; r < runs.size() && s < samples.size(); ++r)
        {
            const auto from = juce::jmax (1u, runs[r].firstChunk);
            const auto to = r + 1 < runs.size() ? runs[r + 1].firstChunk : chunkCount + 1;

            for (auto chunk = from; chunk < to && chunk <= chunkCount && s < samples.size(); ++chunk)
            {
                auto offset = chunks[chunk - 1];

                for (juce::uint32 k = 0; k < runs[r].perChunk && s < samples.size(); ++k, ++s)
                {
                    samples[s].offset = offset;
                    samples[s].size = sizes[s];
                    offset += sizes[s];
                }
            }
        }

        if (s != samples.size())
            return false;

        // Durations
        {
            if (stts->end - stts->begin < 8)
                return false;

            const auto count = be32 (data + stts->begin + 4);

            if (! fits (*stts, 8, count, 8))
                return false;

            size_t at = 0;

            for (juce::uint32 i = 0; i < count && at < samples.size(); ++i)
            {
                const auto n = be32 (data + stts->begin + 8 + 8 * i);
                const auto delta = be32 (data + stts->begin + 12 + 8 * i);

                for (juce::uint32 k = 0; k < n && at < samples.size(); ++k)
                    samples[at++].duration = delta;
            }

            // A last sample without one takes the one before's.
            for (; at < samples.size(); ++at)
                samples[at].duration = at > 0 ? samples[at - 1].duration : 0;
        }

        // Presentation offsets
        if (ctts != nullptr && ctts->end - ctts->begin >= 8)
        {
            const auto count = be32 (data + ctts->begin + 4);

            if (fits (*ctts, 8, count, 8))
            {
                size_t at = 0;

                for (juce::uint32 i = 0; i < count && at < samples.size(); ++i)
                {
                    const auto n = be32 (data + ctts->begin + 8 + 8 * i);
                    const auto offset = static_cast<juce::int32> (be32 (data + ctts->begin + 12 + 8 * i));

                    for (juce::uint32 k = 0; k < n && at < samples.size(); ++k)
                        samples[at++].composition = offset;
                }
            }
        }

        // Key frames (no table: every sample is one)
        if (stss != nullptr && stss->end - stss->begin >= 8)
        {
            const auto count = be32 (data + stss->begin + 4);

            if (fits (*stss, 8, count, 4))
            {
                track.hasSyncTable = true;

                for (auto& sample : samples)
                    sample.sync = false;

                for (juce::uint32 i = 0; i < count; ++i)
                    if (const auto number = be32 (data + stss->begin + 8 + 4 * i); number >= 1 && number <= samples.size())
                        samples[number - 1].sync = true;
            }
        }

        return true;
    }

    bool readTrack (const juce::uint8* data, const Box& trak, Mp4::Track& track, bool withSamples)
    {
        const auto boxes = boxesIn (data, trak.begin, trak.end);

        if (const auto* tkhd = findBox (boxes, fourcc ("tkhd")); tkhd != nullptr && tkhd->end - tkhd->begin >= 24)
        {
            track.trackHeader.replaceAll (data + tkhd->start, tkhd->end - tkhd->start);
            const auto version = data[tkhd->begin];
            track.id = be32 (data + tkhd->begin + (version == 1 ? 20 : 12));
        }

        if (const auto elst = descend (data, trak, { fourcc ("edts"), fourcc ("elst") }); elst && elst->end - elst->begin >= 8)
        {
            const auto version = data[elst->begin];
            const auto count = be32 (data + elst->begin + 4);
            const size_t each = version == 1 ? 20 : 12;

            if ((elst->end - elst->begin - 8) / each >= count)
            {
                for (juce::uint32 i = 0; i < count; ++i)
                {
                    const auto* p = data + elst->begin + 8 + each * i;
                    Mp4::Edit edit;
                    edit.segmentDuration = version == 1 ? static_cast<juce::int64> (be64 (p)) : be32 (p);
                    edit.mediaTime = version == 1 ? static_cast<juce::int64> (be64 (p + 8)) : static_cast<juce::int32> (be32 (p + 4));
                    edit.rateInteger = static_cast<juce::int16> (be16 (p + each - 4));
                    edit.rateFraction = static_cast<juce::int16> (be16 (p + each - 2));
                    track.edits.push_back (edit);
                }
            }
        }

        const auto* mdia = findBox (boxes, fourcc ("mdia"));

        if (mdia == nullptr)
            return false;

        const auto media = boxesIn (data, mdia->begin, mdia->end);

        if (const auto* mdhd = findBox (media, fourcc ("mdhd")); mdhd != nullptr && mdhd->end - mdhd->begin >= 24)
        {
            const auto version = data[mdhd->begin];

            if (version == 1 && mdhd->end - mdhd->begin >= 36)
            {
                track.timescale = be32 (data + mdhd->begin + 20);
                track.mediaDuration = be64 (data + mdhd->begin + 24);
                track.language = be16 (data + mdhd->begin + 32);
            }
            else
            {
                track.timescale = be32 (data + mdhd->begin + 12);
                track.mediaDuration = be32 (data + mdhd->begin + 16);
                track.language = be16 (data + mdhd->begin + 20);
            }
        }

        if (const auto* hdlr = findBox (media, fourcc ("hdlr")); hdlr != nullptr && hdlr->end - hdlr->begin >= 12)
            track.handler = be32 (data + hdlr->begin + 8);

        const auto stbl = descend (data, *mdia, { fourcc ("minf"), fourcc ("stbl") });

        if (! stbl || track.timescale == 0)
            return false;

        if (const auto stsd = descend (data, *stbl, { fourcc ("stsd") }); stsd && stsd->end - stsd->begin >= 16)
        {
            const auto entryStart = stsd->begin + 8;
            const auto size = be32 (data + entryStart);

            if (size >= 8 && size <= stsd->end - entryStart)
            {
                track.codec = be32 (data + entryStart + 4);
                track.sampleEntry.replaceAll (data + entryStart, size);

                if (track.codec == fourcc ("mp4a"))
                    track.objectType = objectTypeOf (data + entryStart, size);
            }
        }

        return ! withSamples || readTables (data, *stbl, track);
    }

    //==============================================================================
    /** Bytes for a new file, big-endian, with boxes that know their sizes. */
    struct Writer
    {
        std::vector<juce::uint8> bytes;

        void u8 (juce::uint32 v)         { bytes.push_back (static_cast<juce::uint8> (v)); }
        void u16 (juce::uint32 v)        { u8 (v >> 8); u8 (v); }
        void u24 (juce::uint32 v)        { u8 (v >> 16); u16 (v & 0xffff); }
        void u32 (juce::uint32 v)        { u16 (v >> 16); u16 (v & 0xffff); }
        void u64 (juce::uint64 v)        { u32 (static_cast<juce::uint32> (v >> 32)); u32 (static_cast<juce::uint32> (v)); }
        void raw (const void* data, size_t size)
        {
            const auto* p = static_cast<const juce::uint8*> (data);
            bytes.insert (bytes.end(), p, p + size);
        }
        void type (const char* name)     { raw (name, 4); }
        void zeros (size_t n)            { bytes.insert (bytes.end(), n, 0); }

        size_t begin (const char* name)
        {
            const auto at = bytes.size();
            u32 (0);
            type (name);
            return at;
        }

        size_t beginFull (const char* name, juce::uint32 version, juce::uint32 flags)
        {
            const auto at = begin (name);
            u8 (version);
            u24 (flags);
            return at;
        }

        void end (size_t at)
        {
            const auto size = static_cast<juce::uint32> (bytes.size() - at);
            bytes[at] = static_cast<juce::uint8> (size >> 24);
            bytes[at + 1] = static_cast<juce::uint8> (size >> 16);
            bytes[at + 2] = static_cast<juce::uint8> (size >> 8);
            bytes[at + 3] = static_cast<juce::uint8> (size);
        }

        void put32At (size_t at, juce::uint32 v)
        {
            bytes[at] = static_cast<juce::uint8> (v >> 24);
            bytes[at + 1] = static_cast<juce::uint8> (v >> 16);
            bytes[at + 2] = static_cast<juce::uint8> (v >> 8);
            bytes[at + 3] = static_cast<juce::uint8> (v);
        }

        void put64At (size_t at, juce::uint64 v)
        {
            put32At (at, static_cast<juce::uint32> (v >> 32));
            put32At (at + 4, static_cast<juce::uint32> (v));
        }

        void matrix()
        {
            for (const auto v : { 0x00010000u, 0u, 0u, 0u, 0x00010000u, 0u, 0u, 0u, 0x40000000u })
                u32 (v);
        }
    };

    /** A track's samples gathered into chunks of about half a second, for the new file. */
    struct Chunk
    {
        int track = 0;                   // 0 the picture, 1 the sound
        size_t first = 0, count = 0;
        double time = 0.0;               // when its first sample is decoded, in seconds
        juce::int64 offset = 0;          // where it goes in the new file
    };

    template <typename DurationOf>
    void chunksOf (int track, size_t count, double timescale, DurationOf durationOf, std::vector<Chunk>& chunks)
    {
        constexpr double chunkSeconds = 0.5;
        constexpr size_t maxPerChunk = 256;
        double time = 0.0;
        Chunk current { track, 0, 0, 0.0, 0 };

        for (size_t i = 0; i < count; ++i)
        {
            if (current.count > 0 && (time >= current.time + chunkSeconds || current.count >= maxPerChunk))
            {
                chunks.push_back (current);
                current = { track, i, 0, time, 0 };
            }

            ++current.count;
            time += durationOf (i) / timescale;
        }

        if (current.count > 0)
            chunks.push_back (current);
    }

    void writeChunkTables (Writer& w, const std::vector<Chunk>& chunks, int track, bool wide)
    {
        std::vector<const Chunk*> mine;

        for (const auto& chunk : chunks)
            if (chunk.track == track)
                mine.push_back (&chunk);

        // stsc: a run each time the count of samples in a chunk changes
        std::vector<std::pair<juce::uint32, juce::uint32>> runs;

        for (size_t i = 0; i < mine.size(); ++i)
            if (runs.empty() || runs.back().second != mine[i]->count)
                runs.push_back ({ static_cast<juce::uint32> (i + 1), static_cast<juce::uint32> (mine[i]->count) });

        auto box = w.beginFull ("stsc", 0, 0);
        w.u32 (static_cast<juce::uint32> (runs.size()));

        for (const auto& run : runs)
        {
            w.u32 (run.first);
            w.u32 (run.second);
            w.u32 (1);
        }

        w.end (box);

        box = w.beginFull (wide ? "co64" : "stco", 0, 0);
        w.u32 (static_cast<juce::uint32> (mine.size()));

        for (const auto* chunk : mine)
        {
            if (wide) w.u64 (static_cast<juce::uint64> (chunk->offset));
            else      w.u32 (static_cast<juce::uint32> (chunk->offset));
        }

        w.end (box);
    }

    void writeDataReference (Writer& w)
    {
        const auto dinf = w.begin ("dinf");
        const auto dref = w.beginFull ("dref", 0, 0);
        w.u32 (1);
        const auto url = w.beginFull ("url ", 0, 1);   // the data is in this file
        w.end (url);
        w.end (dref);
        w.end (dinf);
    }

    void writeHandler (Writer& w, const char* type, const char* name)
    {
        const auto hdlr = w.beginFull ("hdlr", 0, 0);
        w.u32 (0);
        w.type (type);
        w.zeros (12);
        w.raw (name, std::strlen (name) + 1);
        w.end (hdlr);
    }

    void writeMediaHeader (Writer& w, juce::uint32 timescale, juce::uint64 duration, juce::uint16 language)
    {
        const bool wide = duration > 0xffffffffu;
        const auto mdhd = w.beginFull ("mdhd", wide ? 1 : 0, 0);

        if (wide) { w.u64 (0); w.u64 (0); w.u32 (timescale); w.u64 (duration); }
        else      { w.u32 (0); w.u32 (0); w.u32 (timescale); w.u32 (static_cast<juce::uint32> (duration)); }

        w.u16 (language);
        w.u16 (0);
        w.end (mdhd);
    }

    /** The source's track header with its id, duration and flags made right for the new file. */
    void writeTrackHeader (Writer& w, const juce::MemoryBlock& source, juce::uint32 id, juce::uint64 duration)
    {
        const auto* p = static_cast<const juce::uint8*> (source.getData());
        const auto size = source.getSize();
        const auto header = (size >= 16 && be32 (p) == 1) ? 16u : 8u;

        if (size < header + 84)
        {
            // None to copy: a plain one.
            const auto tkhd = w.beginFull ("tkhd", 0, 7);
            w.u32 (0); w.u32 (0); w.u32 (id); w.u32 (0); w.u32 (static_cast<juce::uint32> (duration));
            w.zeros (8); w.u16 (0); w.u16 (0); w.u16 (0); w.u16 (0);
            w.matrix();
            w.u32 (0); w.u32 (0);
            w.end (tkhd);
            return;
        }

        const auto version = p[header];
        const bool wide = version == 1;
        const auto at = w.bytes.size();
        w.u32 (static_cast<juce::uint32> (size - header + 8));
        w.type ("tkhd");
        w.raw (p + header, size - header);

        const auto payload = at + 8;
        w.bytes[payload + 3] |= 0x03;                                         // enabled, in the movie

        if (wide)
        {
            w.put32At (payload + 20, id);
            w.put64At (payload + 28, duration);
        }
        else
        {
            w.put32At (payload + 12, id);
            w.put32At (payload + 20, static_cast<juce::uint32> (juce::jmin<juce::uint64> (duration, 0xffffffffu)));
        }
    }

    void writeDescriptor (Writer& w, juce::uint32 tag, juce::uint32 length)
    {
        w.u8 (tag);
        w.u8 (length);   // every one here is shorter than 128 bytes
    }

    //==============================================================================
    struct Layout
    {
        std::vector<Chunk> chunks;
        juce::uint64 dataSize = 0;
    };

    std::vector<juce::uint8> buildMovieBox (const Mp4::Movie& movie, const Mp4::Track& picture, const Mp4::Mp3Frames& sound,
                                            const std::vector<Chunk>& chunks, bool wide)
    {
        Writer w;
        const auto movieScale = movie.timescale > 0 ? movie.timescale : 1000u;

        // How long each lasts, in the movie's timescale.
        juce::uint64 pictureMedia = 0;

        for (const auto& sample : picture.samples)
            pictureMedia += sample.duration;

        juce::uint64 pictureLength = 0;

        if (! picture.edits.empty())
        {
            for (const auto& edit : picture.edits)
                pictureLength += static_cast<juce::uint64> (juce::jmax<juce::int64> (0, edit.segmentDuration));
        }
        else
        {
            pictureLength = static_cast<juce::uint64> (std::llround (static_cast<double> (pictureMedia) * movieScale / picture.timescale));
        }

        const auto soundMedia = static_cast<juce::uint64> (sound.offsets.size()) * static_cast<juce::uint64> (sound.samplesPerFrame);
        const auto soundFull = static_cast<juce::uint64> (std::llround (static_cast<double> (soundMedia) * movieScale / sound.sampleRate));

        // The sound ends with the picture (its last MP3 frame runs a little over).
        const bool trimSound = soundFull > pictureLength && pictureLength > 0;
        const auto soundLength = trimSound ? pictureLength : soundFull;
        const auto movieLength = juce::jmax (pictureLength, soundLength);

        const auto moov = w.begin ("moov");

        // mvhd
        {
            const bool wideTime = movieLength > 0xffffffffu;
            const auto mvhd = w.beginFull ("mvhd", wideTime ? 1 : 0, 0);

            if (wideTime) { w.u64 (0); w.u64 (0); w.u32 (movieScale); w.u64 (movieLength); }
            else          { w.u32 (0); w.u32 (0); w.u32 (movieScale); w.u32 (static_cast<juce::uint32> (movieLength)); }

            w.u32 (0x00010000);   // rate
            w.u16 (0x0100);       // volume
            w.zeros (10);
            w.matrix();
            w.zeros (24);
            w.u32 (3);            // next track id
            w.end (mvhd);
        }

        // The picture
        {
            const auto trak = w.begin ("trak");
            writeTrackHeader (w, picture.trackHeader, 1, pictureLength);

            if (! picture.edits.empty())
            {
                bool wideEdits = false;

                for (const auto& edit : picture.edits)
                    wideEdits = wideEdits || edit.segmentDuration > 0xffffffffLL || edit.mediaTime > 0x7fffffffLL;

                const auto edts = w.begin ("edts");
                const auto elst = w.beginFull ("elst", wideEdits ? 1 : 0, 0);
                w.u32 (static_cast<juce::uint32> (picture.edits.size()));

                for (const auto& edit : picture.edits)
                {
                    if (wideEdits)
                    {
                        w.u64 (static_cast<juce::uint64> (edit.segmentDuration));
                        w.u64 (static_cast<juce::uint64> (edit.mediaTime));
                    }
                    else
                    {
                        w.u32 (static_cast<juce::uint32> (edit.segmentDuration));
                        w.u32 (static_cast<juce::uint32> (static_cast<juce::int32> (edit.mediaTime)));
                    }

                    w.u16 (static_cast<juce::uint16> (edit.rateInteger));
                    w.u16 (static_cast<juce::uint16> (edit.rateFraction));
                }

                w.end (elst);
                w.end (edts);
            }

            const auto mdia = w.begin ("mdia");
            writeMediaHeader (w, picture.timescale, pictureMedia, picture.language);
            writeHandler (w, "vide", "VideoHandler");

            const auto minf = w.begin ("minf");
            const auto vmhd = w.beginFull ("vmhd", 0, 1);
            w.zeros (8);
            w.end (vmhd);
            writeDataReference (w);

            const auto stbl = w.begin ("stbl");
            auto box = w.beginFull ("stsd", 0, 0);
            w.u32 (1);
            w.raw (picture.sampleEntry.getData(), picture.sampleEntry.getSize());
            w.end (box);

            // stts
            {
                std::vector<std::pair<juce::uint32, juce::uint32>> runs;

                for (const auto& sample : picture.samples)
                {
                    if (! runs.empty() && runs.back().second == sample.duration)
                        ++runs.back().first;
                    else
                        runs.push_back ({ 1u, sample.duration });
                }

                box = w.beginFull ("stts", 0, 0);
                w.u32 (static_cast<juce::uint32> (runs.size()));

                for (const auto& run : runs) { w.u32 (run.first); w.u32 (run.second); }

                w.end (box);
            }

            // ctts
            {
                bool any = false, negative = false;

                for (const auto& sample : picture.samples)
                {
                    any = any || sample.composition != 0;
                    negative = negative || sample.composition < 0;
                }

                if (any)
                {
                    std::vector<std::pair<juce::uint32, juce::int32>> runs;

                    for (const auto& sample : picture.samples)
                    {
                        if (! runs.empty() && runs.back().second == sample.composition)
                            ++runs.back().first;
                        else
                            runs.push_back ({ 1u, sample.composition });
                    }

                    box = w.beginFull ("ctts", negative ? 1 : 0, 0);
                    w.u32 (static_cast<juce::uint32> (runs.size()));

                    for (const auto& run : runs) { w.u32 (run.first); w.u32 (static_cast<juce::uint32> (run.second)); }

                    w.end (box);
                }
            }

            // stss
            if (picture.hasSyncTable)
            {
                std::vector<juce::uint32> keys;

                for (size_t i = 0; i < picture.samples.size(); ++i)
                    if (picture.samples[i].sync)
                        keys.push_back (static_cast<juce::uint32> (i + 1));

                box = w.beginFull ("stss", 0, 0);
                w.u32 (static_cast<juce::uint32> (keys.size()));

                for (const auto key : keys)
                    w.u32 (key);

                w.end (box);
            }

            // stsz
            box = w.beginFull ("stsz", 0, 0);
            w.u32 (0);
            w.u32 (static_cast<juce::uint32> (picture.samples.size()));

            for (const auto& sample : picture.samples)
                w.u32 (sample.size);

            w.end (box);

            writeChunkTables (w, chunks, 0, wide);
            w.end (stbl);
            w.end (minf);
            w.end (mdia);
            w.end (trak);
        }

        // The sound
        {
            const auto trak = w.begin ("trak");

            const auto tkhd = w.beginFull ("tkhd", soundLength > 0xffffffffu ? 1 : 0, 7);

            if (soundLength > 0xffffffffu) { w.u64 (0); w.u64 (0); w.u32 (2); w.u32 (0); w.u64 (soundLength); }
            else                           { w.u32 (0); w.u32 (0); w.u32 (2); w.u32 (0); w.u32 (static_cast<juce::uint32> (soundLength)); }

            w.zeros (8);
            w.u16 (0);          // layer
            w.u16 (0);          // alternate group
            w.u16 (0x0100);     // volume
            w.u16 (0);
            w.matrix();
            w.u32 (0);
            w.u32 (0);
            w.end (tkhd);

            if (trimSound)
            {
                const auto edts = w.begin ("edts");
                const bool wideEdit = soundLength > 0xffffffffu;
                const auto elst = w.beginFull ("elst", wideEdit ? 1 : 0, 0);
                w.u32 (1);

                if (wideEdit) { w.u64 (soundLength); w.u64 (0); }
                else          { w.u32 (static_cast<juce::uint32> (soundLength)); w.u32 (0); }

                w.u16 (1);
                w.u16 (0);
                w.end (elst);
                w.end (edts);
            }

            const auto mdia = w.begin ("mdia");
            writeMediaHeader (w, static_cast<juce::uint32> (sound.sampleRate), soundMedia, 0x55c4);
            writeHandler (w, "soun", "SoundHandler");

            const auto minf = w.begin ("minf");
            const auto smhd = w.beginFull ("smhd", 0, 0);
            w.u32 (0);
            w.end (smhd);
            writeDataReference (w);

            const auto stbl = w.begin ("stbl");
            auto box = w.beginFull ("stsd", 0, 0);
            w.u32 (1);

            const auto mp4a = w.begin ("mp4a");
            w.zeros (6);
            w.u16 (1);                                        // data reference
            w.zeros (8);
            w.u16 (static_cast<juce::uint32> (sound.channels));
            w.u16 (16);
            w.u16 (0);
            w.u16 (0);
            w.u32 (static_cast<juce::uint32> (sound.sampleRate) << 16);

            juce::uint32 biggest = 0;
            juce::uint64 bytes = 0;

            for (const auto size : sound.sizes)
            {
                biggest = juce::jmax (biggest, size);
                bytes += size;
            }

            const auto seconds = static_cast<double> (soundMedia) / sound.sampleRate;
            const auto bitrate = seconds > 0.0 ? static_cast<juce::uint32> (static_cast<double> (bytes) * 8.0 / seconds) : 0u;

            const auto esds = w.beginFull ("esds", 0, 0);
            writeDescriptor (w, 0x03, 3 + 15 + 3);
            w.u16 (0);                                        // ES id
            w.u8 (0);                                         // flags
            writeDescriptor (w, 0x04, 13);
            w.u8 (static_cast<juce::uint32> (sound.objectType));
            w.u8 ((0x05 << 2) | 1);                           // an audio stream
            w.u24 (biggest);
            w.u32 (bitrate);
            w.u32 (bitrate);
            writeDescriptor (w, 0x06, 1);
            w.u8 (0x02);
            w.end (esds);
            w.end (mp4a);
            w.end (box);

            box = w.beginFull ("stts", 0, 0);
            w.u32 (1);
            w.u32 (static_cast<juce::uint32> (sound.offsets.size()));
            w.u32 (static_cast<juce::uint32> (sound.samplesPerFrame));
            w.end (box);

            box = w.beginFull ("stsz", 0, 0);
            w.u32 (0);
            w.u32 (static_cast<juce::uint32> (sound.sizes.size()));

            for (const auto size : sound.sizes)
                w.u32 (size);

            w.end (box);

            writeChunkTables (w, chunks, 1, wide);
            w.end (stbl);
            w.end (minf);
            w.end (mdia);
            w.end (trak);
        }

        w.end (moov);
        return std::move (w.bytes);
    }

    //==============================================================================
    constexpr int mp3Bitrates[2][16] = {
        { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 },   // MPEG-1 Layer III
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }        // MPEG-2 and 2.5 Layer III
    };

    constexpr int mp3Rates[3][3] = {
        { 44100, 48000, 32000 },     // MPEG-1
        { 22050, 24000, 16000 },     // MPEG-2
        { 11025, 12000, 8000 }       // MPEG-2.5
    };

    struct Mp3Header
    {
        int length = 0, sampleRate = 0, channels = 0, samples = 0, sideInfo = 0;
        bool mpeg1 = false;
    };

    std::optional<Mp3Header> mp3HeaderAt (const juce::uint8* p)
    {
        if (p[0] != 0xff || (p[1] & 0xe0) != 0xe0)
            return std::nullopt;

        const auto version = (p[1] >> 3) & 3;       // 3 MPEG-1, 2 MPEG-2, 0 MPEG-2.5
        const auto layer = (p[1] >> 1) & 3;         // 1: Layer III
        const auto bitrateIndex = (p[2] >> 4) & 15;
        const auto rateIndex = (p[2] >> 2) & 3;
        const auto padding = (p[2] >> 1) & 1;
        const auto mode = (p[3] >> 6) & 3;

        if (version == 1 || layer != 1 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3)
            return std::nullopt;

        Mp3Header header;
        header.mpeg1 = version == 3;
        header.sampleRate = mp3Rates[version == 3 ? 0 : version == 2 ? 1 : 2][rateIndex];
        const auto kbps = mp3Bitrates[header.mpeg1 ? 0 : 1][bitrateIndex];
        header.samples = header.mpeg1 ? 1152 : 576;
        header.length = (header.mpeg1 ? 144 : 72) * kbps * 1000 / header.sampleRate + padding;
        header.channels = mode == 3 ? 1 : 2;
        header.sideInfo = header.mpeg1 ? (header.channels == 1 ? 17 : 32) : (header.channels == 1 ? 9 : 17);
        return header;
    }
}

//==============================================================================
const Mp4::Track* Mp4::Movie::first (juce::uint32 handler) const noexcept
{
    // The first one in the movie (enabled, when any is), as players choose.
    const Track* found = nullptr;

    for (const auto& track : tracks)
    {
        if (track.handler != handler)
            continue;

        const auto* header = static_cast<const juce::uint8*> (track.trackHeader.getData());
        const auto headerSize = track.trackHeader.getSize();
        const auto offset = (headerSize >= 16 && be32 (header) == 1) ? 16u : 8u;
        const bool enabled = headerSize > offset + 3 && (header[offset + 3] & 1) != 0;

        if (enabled)
            return &track;

        if (found == nullptr)
            found = &track;
    }

    return found;
}

std::optional<Mp4::Movie> Mp4::read (const juce::File& file, bool withSamples)
{
    juce::FileInputStream in (file);

    if (! in.openedOk())
        return std::nullopt;

    const auto total = in.getTotalLength();
    juce::int64 position = 0, moovAt = -1, moovSize = 0;
    bool fragments = false, first = true;

    while (position + 8 <= total)
    {
        juce::uint8 header[16];
        in.setPosition (position);

        if (in.read (header, 8) != 8)
            break;

        juce::uint64 size = be32 (header);
        const auto type = be32 (header + 4);
        juce::int64 headerSize = 8;

        if (size == 1)
        {
            if (in.read (header + 8, 8) != 8)
                break;

            size = be64 (header + 8);
            headerSize = 16;
        }
        else if (size == 0)
        {
            size = static_cast<juce::uint64> (total - position);
        }

        // An MP4 or QuickTime file starts with one of these.
        if (first)
        {
            static const juce::uint32 starts[] = { fourcc ("ftyp"), fourcc ("moov"), fourcc ("mdat"), fourcc ("wide"),
                                                   fourcc ("free"), fourcc ("skip"), fourcc ("pnot"), fourcc ("uuid") };

            if (std::find (std::begin (starts), std::end (starts), type) == std::end (starts))
                return std::nullopt;

            first = false;
        }

        if (size < static_cast<juce::uint64> (headerSize) || size > static_cast<juce::uint64> (total - position))
            break;

        if (type == fourcc ("moov"))
        {
            moovAt = position;
            moovSize = static_cast<juce::int64> (size);
        }
        else if (type == fourcc ("moof"))
        {
            fragments = true;
        }

        position += static_cast<juce::int64> (size);
    }

    if (moovAt < 0 || moovSize > 512 * 1024 * 1024)
        return std::nullopt;

    juce::MemoryBlock block;
    in.setPosition (moovAt);

    if (in.readIntoMemoryBlock (block, moovSize) != static_cast<size_t> (moovSize))
        return std::nullopt;

    const auto* data = static_cast<const juce::uint8*> (block.getData());
    const auto top = boxesIn (data, 0, block.getSize());

    if (top.empty() || top.front().type != fourcc ("moov"))
        return std::nullopt;

    Movie movie;
    movie.fragmented = fragments;
    const auto children = boxesIn (data, top.front().begin, top.front().end);

    for (const auto& box : children)
    {
        if (box.type == fourcc ("mvhd") && box.end - box.begin >= 20)
        {
            const auto version = data[box.begin];

            if (version == 1 && box.end - box.begin >= 32)
            {
                movie.timescale = be32 (data + box.begin + 20);
                movie.duration = be64 (data + box.begin + 24);
            }
            else
            {
                movie.timescale = be32 (data + box.begin + 12);
                movie.duration = be32 (data + box.begin + 16);
            }
        }
        else if (box.type == fourcc ("mvex"))
        {
            movie.fragmented = true;
        }
        else if (box.type == fourcc ("trak"))
        {
            Track track;

            if (readTrack (data, box, track, withSamples))
                movie.tracks.push_back (std::move (track));
        }
    }

    return movie;
}

bool Mp4::canCopyPicture (juce::uint32 codec) noexcept
{
    return codec == fourcc ("avc1") || codec == fourcc ("avc3") || codec == fourcc ("hvc1") || codec == fourcc ("hev1")
        || codec == fourcc ("mp4v") || codec == fourcc ("av01") || codec == fourcc ("vp09");
}

int Mp4::soundCorrection (const juce::File& video, double soundRate)
{
    if (soundRate <= 0.0)
        return 0;

    if (const auto movie = read (video, false))
    {
        const auto* sound = movie->first (fourcc ("soun"));

        if (sound == nullptr || sound->timescale == 0 || movie->timescale == 0)
            return 0;

        // Empty edits first: the sound starts that much after the picture.
        double gap = 0.0;
        juce::int64 mediaTime = 0;

        for (const auto& edit : sound->edits)
        {
            if (edit.mediaTime < 0)
            {
                gap += static_cast<double> (edit.segmentDuration) / movie->timescale;
                continue;
            }

            mediaTime = edit.mediaTime;
            break;
        }

        const auto scale = soundRate / sound->timescale;
        const auto skip = static_cast<double> (mediaTime) * scale;   // what the file says to leave out
        double correction = gap * soundRate;

        const bool aac = sound->codec == fourcc ("mp4a") && (sound->objectType == 0x40 || sound->objectType == 0x66
                                                             || sound->objectType == 0x67 || sound->objectType == 0x68);

        if (aac)
        {
            // VLC's AAC leaves out a frame more than the file says, and the edit's start in
            // whole frames.
            const auto frame = 1024.0 * scale;
            const auto vlcLeavesOut = frame + frame * std::floor (static_cast<double> (mediaTime) / 1024.0);
            correction += vlcLeavesOut - skip;
        }
        else
        {
            // Other kinds: VLC keeps what the edit says to leave out.
            correction -= skip;
        }

        return static_cast<int> (std::lround (correction));
    }

    // Matroska, WebM, Ogg: an Opus stream's pre-skip, which VLC keeps.
    juce::FileInputStream in (video);

    if (! in.openedOk())
        return 0;

    juce::MemoryBlock head;
    in.readIntoMemoryBlock (head, 1024 * 1024);
    const auto* data = static_cast<const juce::uint8*> (head.getData());

    for (size_t i = 0; i + 12 <= head.getSize(); ++i)
    {
        if (std::memcmp (data + i, "OpusHead", 8) == 0)
        {
            const auto preSkip = static_cast<int> (data[i + 10] | (data[i + 11] << 8));
            return -static_cast<int> (std::lround (preSkip * soundRate / 48000.0));
        }
    }

    return 0;
}

//==============================================================================
std::optional<Mp4::Mp3Frames> Mp4::readMp3 (const juce::File& file)
{
    juce::MemoryBlock block;

    if (! file.loadFileAsData (block))
        return std::nullopt;

    const auto* data = static_cast<const juce::uint8*> (block.getData());
    const auto size = block.getSize();
    size_t position = 0;

    // An ID3v2 tag first
    if (size >= 10 && std::memcmp (data, "ID3", 3) == 0)
        position = 10 + ((data[6] & 0x7f) << 21 | (data[7] & 0x7f) << 14 | (data[8] & 0x7f) << 7 | (data[9] & 0x7f));

    Mp3Frames frames;
    bool firstFrame = true;

    while (position + 4 <= size)
    {
        const auto header = mp3HeaderAt (data + position);

        if (! header || header->length <= 0 || position + static_cast<size_t> (header->length) > size)
        {
            if (frames.offsets.empty() && firstFrame)
            {
                ++position;     // looking for the first frame
                continue;
            }

            break;
        }

        // The tag frame (Xing / Info / VBRI) holds no sound.
        if (firstFrame)
        {
            firstFrame = false;
            const auto tagAt = position + 4 + static_cast<size_t> (header->sideInfo);

            if (tagAt + 4 <= size && (std::memcmp (data + tagAt, "Xing", 4) == 0 || std::memcmp (data + tagAt, "Info", 4) == 0
                                      || std::memcmp (data + position + 36, "VBRI", 4) == 0))
            {
                position += static_cast<size_t> (header->length);
                continue;
            }
        }

        if (frames.offsets.empty())
        {
            frames.sampleRate = header->sampleRate;
            frames.channels = header->channels;
            frames.samplesPerFrame = header->samples;
            frames.objectType = header->mpeg1 ? 0x6b : 0x69;
        }
        else if (header->sampleRate != frames.sampleRate)
        {
            break;
        }

        frames.offsets.push_back (static_cast<juce::int64> (position));
        frames.sizes.push_back (static_cast<juce::uint32> (header->length));
        position += static_cast<size_t> (header->length);
    }

    if (frames.offsets.empty())
        return std::nullopt;

    return frames;
}

//==============================================================================
juce::Result Mp4::write (const juce::File& target, const juce::File& pictureFile, const Movie& movie,
                         const juce::File& mp3File, const Mp3Frames& sound,
                         const std::function<void (double)>& progress, std::atomic<bool>* cancelled)
{
    const auto* picture = movie.first (fourcc ("vide"));

    if (picture == nullptr || picture->samples.empty() || picture->sampleEntry.getSize() == 0)
        return juce::Result::fail ("The video's picture couldn't be read.");

    if (sound.offsets.empty() || sound.sampleRate <= 0)
        return juce::Result::fail ("The mix couldn't be read.");

    // Both tracks in chunks of half a second, in the order they're played.
    std::vector<Chunk> chunks;
    const auto& samples = picture->samples;
    chunksOf (0, samples.size(), picture->timescale, [&samples] (size_t i) { return static_cast<double> (samples[i].duration); }, chunks);
    chunksOf (1, sound.offsets.size(), sound.sampleRate, [&sound] (size_t) { return static_cast<double> (sound.samplesPerFrame); }, chunks);
    std::stable_sort (chunks.begin(), chunks.end(), [] (const Chunk& a, const Chunk& b) { return a.time < b.time; });

    juce::uint64 dataSize = 0;

    for (const auto& sample : samples)
        dataSize += sample.size;

    for (const auto size : sound.sizes)
        dataSize += size;

    // ftyp
    Writer head;
    {
        const auto ftyp = head.begin ("ftyp");
        head.type ("isom");
        head.u32 (0x200);

        for (const auto* brand : { "isom", "iso2", "avc1", "mp41" })
            head.type (brand);

        head.end (ftyp);
    }

    // The movie box goes first (so the file plays while it's still loading); its size doesn't
    // depend on where the chunks are, so it's made once to measure, then for real.
    const bool wide = dataSize + 64 * 1024 * 1024 > 0xffffffffull;
    auto moov = buildMovieBox (movie, *picture, sound, chunks, wide);
    const auto dataStart = static_cast<juce::int64> (head.bytes.size() + moov.size() + 16);
    auto offset = dataStart;

    for (auto& chunk : chunks)
    {
        chunk.offset = offset;

        if (chunk.track == 0)
            for (size_t i = chunk.first; i < chunk.first + chunk.count; ++i)
                offset += samples[i].size;
        else
            for (size_t i = chunk.first; i < chunk.first + chunk.count; ++i)
                offset += sound.sizes[i];
    }

    moov = buildMovieBox (movie, *picture, sound, chunks, wide);

    juce::FileInputStream pictureIn (pictureFile), soundIn (mp3File);

    if (! pictureIn.openedOk() || ! soundIn.openedOk())
        return juce::Result::fail ("The video or the mix couldn't be read.");

    juce::TemporaryFile temporary (target);

    {
        juce::FileOutputStream out (temporary.getFile());

        if (! out.openedOk())
            return juce::Result::fail ("Couldn't write " + target.getFullPathName());

        out.write (head.bytes.data(), head.bytes.size());
        out.write (moov.data(), moov.size());

        Writer mdat;
        mdat.u32 (1);
        mdat.type ("mdat");
        mdat.u64 (dataSize + 16);
        out.write (mdat.bytes.data(), mdat.bytes.size());

        juce::HeapBlock<char> buffer;
        size_t bufferSize = 0;
        size_t done = 0;

        const auto copy = [&] (juce::FileInputStream& in, juce::int64 from, juce::uint32 size)
        {
            if (bufferSize < size)
            {
                bufferSize = size;
                buffer.realloc (bufferSize);
            }

            return in.setPosition (from) && in.read (buffer.get(), static_cast<int> (size)) == static_cast<int> (size)
                && out.write (buffer.get(), size);
        };

        for (const auto& chunk : chunks)
        {
            if (cancelled != nullptr && cancelled->load())
                return juce::Result::fail ("Cancelled");

            for (size_t i = chunk.first; i < chunk.first + chunk.count; ++i)
            {
                const bool ok = chunk.track == 0 ? copy (pictureIn, samples[i].offset, samples[i].size)
                                                 : copy (soundIn, sound.offsets[i], sound.sizes[i]);

                if (! ok)
                    return juce::Result::fail (chunk.track == 0 ? "Couldn't copy the video's picture into the new file."
                                                                : "Couldn't copy the video's sound (the mix) into the new file.");
            }

            if (progress)
                progress (static_cast<double> (++done) / static_cast<double> (chunks.size()));
        }

        out.flush();

        if (out.getStatus().failed() || out.getPosition() != dataStart + static_cast<juce::int64> (dataSize))
            return juce::Result::fail ("Couldn't write " + target.getFullPathName() + " (is the disk full?)");
    }

    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Couldn't write " + target.getFullPathName());

    return juce::Result::ok();
}

//==============================================================================
juce::Result Mp4::Export::run (const juce::File& video, const juce::File& mix, const juce::File& target,
                               std::atomic<double>* progress, std::atomic<bool>* cancelled)
{
    const auto report = [progress] (double value) { if (progress != nullptr) progress->store (value); };
    const auto stopped = [cancelled] { return cancelled != nullptr && cancelled->load(); };

    // The picture: as it is when an MP4 can hold it, otherwise written again by VLC.
    auto movie = read (video, true);
    auto pictureFile = video;
    std::unique_ptr<juce::TemporaryFile> transcoded;
    double soundFrom = 0.0;

    const auto usable = [] (const std::optional<Movie>& m)
    {
        if (! m || m->fragmented)
            return false;

        const auto* picture = m->first (fourcc ("vide"));
        return picture != nullptr && ! picture->samples.empty() && canCopyPicture (picture->codec);
    };

    if (! usable (movie))
    {
        transcoded = std::make_unique<juce::TemporaryFile> (target.withFileExtension (".mp4"), juce::TemporaryFile::useHiddenFile);
        const auto result = VideoMedia::transcodePicture (video, transcoded->getFile(),
                                                          [&report] (double part) { report (0.6 * part); }, cancelled);
        report (0.6);

        if (result.failed())
            return result;

        movie = read (transcoded->getFile(), true);

        if (! usable (movie))
            return juce::Result::fail ("The video's picture couldn't be written as H.264.");

        pictureFile = transcoded->getFile();
        soundFrom = 0.6;
    }

    if (stopped())
        return juce::Result::fail ("Cancelled");

    // The mix as MP3. LAME's output starts 1105 samples late (its encoder and decoder delays),
    // so as many are left off its start: the sound starts with the picture in every player.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (mix));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return juce::Result::fail ("The mix couldn't be read.");

    const auto rate = reader->sampleRate;

    if (rate != 32000.0 && rate != 44100.0 && rate != 48000.0)
        return juce::Result::fail ("The mix must be at 32, 44.1 or 48 kHz for the video.");

    juce::TemporaryFile mp3 (target.withFileExtension (".mp3"), juce::TemporaryFile::useHiddenFile);

    {
        Mp3Encoder encoder;

        if (const auto opened = encoder.open (mp3.getFile(), rate, 320); opened.failed())
            return opened;

        constexpr juce::int64 lameDelay = 576 + 529;
        constexpr int block = 8192;
        juce::AudioBuffer<float> buffer (2, block);
        const auto length = reader->lengthInSamples;

        for (juce::int64 at = juce::jmin (lameDelay, length); at < length; at += block)
        {
            if (stopped())
            {
                encoder.close();
                return juce::Result::fail ("Cancelled");
            }

            const auto count = static_cast<int> (juce::jmin<juce::int64> (block, length - at));
            reader->read (&buffer, 0, count, at, true, true);

            if (reader->numChannels == 1)
                buffer.copyFrom (1, 0, buffer, 0, 0, count);

            if (! encoder.write (buffer.getReadPointer (0), buffer.getReadPointer (1), count))
            {
                encoder.close();
                return juce::Result::fail ("The mix couldn't be written as MP3.");
            }

            report (soundFrom + (1.0 - soundFrom) * 0.5 * static_cast<double> (at) / static_cast<double> (length));
        }

        if (const auto closed = encoder.close(); closed.failed())
            return closed;
    }

    const auto frames = readMp3 (mp3.getFile());

    if (! frames)
        return juce::Result::fail ("The mix couldn't be written as MP3.");

    const auto written = write (target, pictureFile, *movie, mp3.getFile(), *frames,
                                [&] (double part) { report (soundFrom + (1.0 - soundFrom) * (0.5 + 0.5 * part)); }, cancelled);
    report (1.0);
    return written;
}

//==============================================================================
class Mp4::Export::Worker final : public juce::Thread
{
public:
    Worker (const juce::File& v, const juce::File& m, const juce::File& t)
        : juce::Thread ("Sonify Export Video"), video (v), mix (m), target (t) {}

    ~Worker() override
    {
        cancelled = true;
        stopThread (60000);
    }

    void run() override
    {
        const auto outcome = Export::run (video, mix, target, &progress, &cancelled);

        {
            const juce::ScopedLock lock (resultLock);
            result = outcome;
        }

        done = true;
    }

    const juce::File video, mix, target;
    std::atomic<double> progress { 0.0 };
    std::atomic<bool> cancelled { false }, done { false };
    juce::CriticalSection resultLock;
    juce::Result result { juce::Result::ok() };
};

Mp4::Export::Export (const juce::File& video, const juce::File& mix, const juce::File& target)
    : worker (std::make_unique<Worker> (video, mix, target))
{
    worker->startThread();
}

Mp4::Export::~Export() = default;

bool Mp4::Export::isDone() const noexcept        { return worker->done.load(); }
double Mp4::Export::getProgress() const noexcept { return worker->progress.load(); }
void Mp4::Export::cancel()                       { worker->cancelled = true; }

juce::Result Mp4::Export::getResult() const
{
    const juce::ScopedLock lock (worker->resultLock);
    return worker->result;
}
