// D:\workspace\Fanan_web_hub\src\AxisPack.cpp

#include "AxisPack.h"

#include "stella_blob.h"
#include "Licences.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace AxisPack
{

namespace
{
    //==========================================================================
    // The index's lines: tab between the fields, key=value in each, nothing in a value that
    // would end the field or the line.

    juce::String clean (const juce::String& text)
    {
        return text.replaceCharacters ("\t\r\n", "   ").trim();
    }

    juce::String field (const char* key, const juce::String& value)
    {
        return "\t" + juce::String (key) + "=" + clean (value);
    }

    juce::String field (const char* key, double value)
    {
        return "\t" + juce::String (key) + "=" + juce::String (value, 10);
    }

    juce::String field (const char* key, int value)
    {
        return "\t" + juce::String (key) + "=" + juce::String (value);
    }

    /** A line's kind (its first field), and its fields after it as key and value. */
    struct Line
    {
        juce::String kind;
        juce::StringPairArray values;

        juce::String text (const char* key, const juce::String& ifNone = {}) const
        {
            return values.containsKey (key) ? values[key] : ifNone;
        }

        double number (const char* key, double ifNone) const
        {
            return values.containsKey (key) ? values[key].getDoubleValue() : ifNone;
        }

        int integer (const char* key, int ifNone) const
        {
            return values.containsKey (key) ? values[key].getIntValue() : ifNone;
        }
    };

    Line lineOf (const std::string& text)
    {
        Line line;
        const auto fields = juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (text.c_str())), "\t", "");

        if (fields.isEmpty())
            return line;

        line.kind = fields[0].trim();

        for (int i = 1; i < fields.size(); ++i)
        {
            const auto equals = fields[i].indexOfChar ('=');

            if (equals > 0)
                line.values.set (fields[i].substring (0, equals), fields[i].substring (equals + 1));
        }

        return line;
    }

    //==========================================================================
    // NAM's JSON.

    juce::String stringOf (const juce::var& object, const char* key)
    {
        const auto value = object.getProperty (key, {});
        return value.isVoid() || value.isUndefined() ? juce::String() : value.toString();
    }

    double numberOf (const juce::var& object, const char* key, double ifNone)
    {
        const auto value = object.getProperty (key, {});
        return value.isDouble() || value.isInt() || value.isInt64() ? (double) value : ifNone;
    }

    /** A JSON array of numbers as floats. False: it isn't one, or it's empty. */
    bool weightsOf (const juce::var& value, std::vector<float>& into)
    {
        const auto* array = value.getArray();

        if (array == nullptr || array->isEmpty())
            return false;

        into.clear();
        into.reserve ((size_t) array->size());

        for (const auto& number : *array)
        {
            if (! (number.isDouble() || number.isInt() || number.isInt64()))
                return false;

            into.push_back ((float) (double) number);
        }

        return true;
    }

    /** One model (a whole .nam, or one of a SlimmableContainer's) as a submodel. */
    bool submodelOf (const juce::var& model, double maxValue, Capture::Submodel& into, juce::String& error)
    {
        into = Capture::Submodel();
        into.architecture = stringOf (model, "architecture");
        into.maxValue = maxValue;

        if (into.architecture.isEmpty())
        {
            error = "one of its models doesn't say what architecture it is";
            return false;
        }

        if (into.architecture.equalsIgnoreCase ("SlimmableContainer"))
        {
            error = "it has a slimmable model inside a slimmable model, which AXIS doesn't read";
            return false;
        }

        const auto config = model.getProperty ("config", {});

        if (! config.isObject())
        {
            error = "its " + into.architecture + " model has no config";
            return false;
        }

        if (! weightsOf (model.getProperty ("weights", {}), into.weights))
        {
            error = "its " + into.architecture + " model has no weights"
                  + (into.architecture.equalsIgnoreCase ("WaveNet") ? " (is it a slimmable file written by hand?)" : "");
            return false;
        }

        // Word for word, as NAM wrote it: AXIS hands it back to whatever plays the model.
        into.config = juce::JSON::toString (config, true, 20);

        const auto metadata = model.getProperty ("metadata", {});
        into.loudness = numberOf (metadata, "loudness", 0.0);
        into.gain = numberOf (metadata, "gain", 1.0);

        return true;
    }

    //==========================================================================
    // IRs.

    /** The frames brought to `toRate` (nothing to do when it's there already). */
    void resample (std::vector<float>& frames, double fromRate, double toRate)
    {
        if (frames.empty() || fromRate <= 0.0 || toRate <= 0.0 || std::abs (fromRate - toRate) < 0.5)
            return;

        const auto ratio = fromRate / toRate;
        const auto wanted = (int) std::ceil ((double) frames.size() / ratio);

        if (wanted <= 0)
        {
            frames.clear();
            return;
        }

        std::vector<float> out ((size_t) wanted, 0.0f);
        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process (ratio, frames.data(), out.data(), wanted, (int) frames.size(), 0);
        frames.swap (out);
    }

    /** The tail past `seconds` cut off, the last of what's kept faded so there's no click. */
    void trim (std::vector<float>& frames, double rate, double seconds)
    {
        const auto most = (size_t) juce::jmax (1.0, rate * seconds);

        if (frames.size() <= most)
            return;

        frames.resize (most);

        const auto fade = (size_t) juce::jmin ((int) most, 64);

        for (size_t i = 0; i < fade; ++i)
            frames[most - fade + i] *= 1.0f - (float) (i + 1) / (float) fade;
    }
}

//==============================================================================
int Capture::widest() const noexcept
{
    return submodels.empty() ? -1 : (int) submodels.size() - 1;
}

int Capture::forSlim (double slim) const noexcept
{
    // SlimmableContainer's own way round: the first width whose maxValue the slim value is inside.
    for (int i = 0; i < (int) submodels.size(); ++i)
        if (slim <= submodels[(size_t) i].maxValue)
            return i;

    return widest();
}

bool Capture::isValid() const noexcept
{
    if (submodels.empty())
        return false;

    for (const auto& submodel : submodels)
        if (! submodel.isValid())
            return false;

    return true;
}

juce::String Capture::displayName() const
{
    if (name.isNotEmpty())
        return name;

    const auto gear = (gearMake + " " + gearModel).trim();
    return gear.isNotEmpty() ? gear : juce::String ("Capture");
}

juce::String Ir::displayName() const
{
    if (name.isNotEmpty())
        return name;

    const auto gear = (gearMake + " " + gearModel).trim();
    return gear.isNotEmpty() ? gear : juce::String ("Cabinet");
}

//==============================================================================
juce::String namWildcards()    { return "*.nam"; }
juce::String irWildcards()     { return "*.wav;*.aiff;*.aif;*.flac"; }

bool isNamFile (const juce::File& file)
{
    return file.hasFileExtension ("nam");
}

bool isIrFile (const juce::File& file)
{
    return file.hasFileExtension ("wav;aiff;aif;flac");
}

//==============================================================================
bool readNamText (const juce::String& json, const juce::String& fallbackName, Capture& into, juce::String& error)
{
    into = Capture();

    juce::var root;

    if (juce::JSON::parse (json, root).failed() || ! root.isObject())
    {
        error = "It isn't a NAM file: its text isn't JSON.";
        return false;
    }

    into.namVersion = stringOf (root, "version");
    into.sampleRate = numberOf (root, "sample_rate", captureRate);

    if (into.sampleRate < 8000.0 || into.sampleRate > 768000.0)
        into.sampleRate = captureRate;

    const auto metadata = root.getProperty ("metadata", {});
    into.name = stringOf (metadata, "name").trim();
    into.maker = stringOf (metadata, "modeled_by").trim();
    into.gearMake = stringOf (metadata, "gear_make").trim();
    into.gearModel = stringOf (metadata, "gear_model").trim();
    into.gearType = stringOf (metadata, "gear_type").trim();
    into.toneType = stringOf (metadata, "tone_type").trim();
    into.loudness = numberOf (metadata, "loudness", 0.0);
    into.gain = numberOf (metadata, "gain", 1.0);

    // The cabinet is in the capture when it was taken through one: AXIS leaves its IR stage out.
    into.hasCab = into.gearType.containsIgnoreCase ("cab");

    // How far it was from the real amp, where training said (metadata.training.validation_esr).
    into.esr = numberOf (metadata.getProperty ("training", {}), "validation_esr", -1.0);

    if (into.name.isEmpty())
        into.name = fallbackName.trim();

    const auto architecture = stringOf (root, "architecture");

    if (architecture.isEmpty())
    {
        error = "It isn't a NAM file: it doesn't say what architecture it is.";
        return false;
    }

    if (architecture.equalsIgnoreCase ("SlimmableContainer"))
    {
        // Not a model: the same amp at several widths, cheapest first, and the file's own weights
        // array is empty. Unwrapped here, so nothing after this has to know.
        // Held while they're read: the array belongs to the file's own tree, not to a passing copy.
        const auto config = root.getProperty ("config", {});
        const auto wrapped = config.getProperty ("submodels", {});
        const auto* submodels = wrapped.getArray();

        if (submodels == nullptr || submodels->isEmpty())
        {
            error = "It's a slimmable NAM file with no models in it.";
            return false;
        }

        for (const auto& one : *submodels)
        {
            Capture::Submodel submodel;
            juce::String why;

            if (! submodelOf (one.getProperty ("model", {}), numberOf (one, "max_value", 1.0), submodel, why))
            {
                error = "Its slimmable models can't be read: " + why + ".";
                return false;
            }

            into.submodels.push_back (std::move (submodel));
        }

        // Cheapest first, as SlimmableContainer means them: the widest last is the one AXIS plays.
        std::stable_sort (into.submodels.begin(), into.submodels.end(),
                          [] (const Capture::Submodel& a, const Capture::Submodel& b)
                          {
                              if (a.maxValue != b.maxValue)
                                  return a.maxValue < b.maxValue;

                              return a.weights.size() < b.weights.size();
                          });

        // A capture's own numbers, when the file didn't give them, are the widest model's.
        if (into.loudness == 0.0)
            into.loudness = into.submodels.back().loudness;

        if (into.gain == 1.0)
            into.gain = into.submodels.back().gain;
    }
    else
    {
        Capture::Submodel submodel;
        juce::String why;

        if (! submodelOf (root, 1.0, submodel, why))
        {
            error = "It can't be read: " + why + ".";
            return false;
        }

        into.submodels.push_back (std::move (submodel));
    }

    if (! into.isValid())
    {
        error = "It has no model AXIS can play.";
        return false;
    }

    return true;
}

bool readNam (const juce::File& file, Capture& into, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = file.getFileName() + " isn't there.";
        return false;
    }

    const auto text = file.loadFileAsString();

    if (text.isEmpty())
    {
        error = file.getFileName() + " is empty.";
        return false;
    }

    if (! readNamText (text, file.getFileNameWithoutExtension(), into, error))
    {
        error = file.getFileName() + ": " + error;
        return false;
    }

    return true;
}

//==============================================================================
bool readIr (const juce::File& file, Conditioning conditioning, Ir& into, juce::String& error)
{
    into = Ir();

    if (! file.existsAsFile())
    {
        error = file.getFileName() + " isn't there.";
        return false;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        error = file.getFileName() + " isn't a sound file Sonify reads.";
        return false;
    }

    if (reader->lengthInSamples <= 0 || reader->numChannels < 1 || reader->sampleRate < 1000.0)
    {
        error = file.getFileName() + " has no sound in it.";
        return false;
    }

    // An IR is short by its nature: a file far longer than a cabinet's tail is something else.
    const auto mostFrames = (juce::int64) (reader->sampleRate * 10.0);

    if (reader->lengthInSamples > mostFrames)
    {
        error = file.getFileName() + " is " + juce::String (reader->lengthInSamples / reader->sampleRate, 1)
              + " seconds long: that's a recording, not an impulse response.";
        return false;
    }

    const auto frames = (int) reader->lengthInSamples;
    const auto channels = reader->numChannels > 1 ? 2 : 1;

    juce::AudioBuffer<float> buffer (channels, frames);
    buffer.clear();

    if (! reader->read (&buffer, 0, frames, 0, true, channels > 1))
    {
        error = file.getFileName() + " couldn't be read.";
        return false;
    }

    into.name = file.getFileNameWithoutExtension().replaceCharacter ('_', ' ').trim();
    into.sampleRate = reader->sampleRate;
    into.left.assign (buffer.getReadPointer (0), buffer.getReadPointer (0) + frames);

    if (channels > 1)
        into.right.assign (buffer.getReadPointer (1), buffer.getReadPointer (1) + frames);

    if (conditioning != Conditioning::asItIs)
    {
        resample (into.left, into.sampleRate, captureRate);

        if (! into.right.empty())
            resample (into.right, into.sampleRate, captureRate);

        into.sampleRate = captureRate;

        trim (into.left, into.sampleRate, irSeconds);

        if (! into.right.empty())
            trim (into.right, into.sampleRate, irSeconds);
    }

    if (into.left.empty())
    {
        error = file.getFileName() + " has nothing left in it once it's trimmed.";
        return false;
    }

    // Both sides the same length, whatever resampling made of them.
    if (! into.right.empty() && into.right.size() != into.left.size())
        into.right.resize (into.left.size(), 0.0f);

    if (conditioning == Conditioning::levelled)
    {
        // One level with every other IR: the energy of the impulse, so putting one cabinet in
        // place of another doesn't change how loud the amp is. Held back from clipping as well.
        double energy = 0.0;
        float peak = 0.0f;

        for (size_t i = 0; i < into.left.size(); ++i)
        {
            energy += (double) into.left[i] * (double) into.left[i];
            peak = juce::jmax (peak, std::abs (into.left[i]));

            if (! into.right.empty())
            {
                energy += (double) into.right[i] * (double) into.right[i];
                peak = juce::jmax (peak, std::abs (into.right[i]));
            }
        }

        energy /= (double) into.channels();

        if (energy > 1.0e-12 && peak > 0.0f)
        {
            auto gain = (float) (1.0 / std::sqrt (energy));
            gain = juce::jmin (gain, 0.999f / peak);

            for (auto& sample : into.left)
                sample *= gain;

            for (auto& sample : into.right)
                sample *= gain;
        }
    }

    if (! into.isValid())
    {
        error = file.getFileName() + " couldn't be made into an impulse response.";
        return false;
    }

    return true;
}

//==============================================================================
namespace
{
    /** A capture's submodel, as a pack's index holds it. */
    struct PackedSubmodel
    {
        juce::String architecture, config;
        double maxValue = 1.0, loudness = 0.0, gain = 1.0;
        int sample = -1, weights = 0;
    };

    struct PackedAmp
    {
        Entry entry;
        juce::String gearType, namVersion;
        double loudness = 0.0, gain = 1.0, esr = -1.0;
        std::vector<PackedSubmodel> submodels;
    };

    struct PackedCab
    {
        Entry entry;
        int sample = -1;
    };

    juce::String base64 (const juce::String& text)
    {
        const auto utf8 = text.toRawUTF8();
        return juce::Base64::toBase64 (utf8, std::strlen (utf8));
    }

    juce::String fromBase64 (const juce::String& text)
    {
        juce::MemoryOutputStream out;

        if (! juce::Base64::convertFromBase64 (out, text))
            return {};

        return juce::String::fromUTF8 (static_cast<const char*> (out.getData()), (int) out.getDataSize());
    }
}

//==============================================================================
class Pack::Impl
{
public:
    juce::File file;
    stella::blob::Head head;
    stella::blob::Cipher cipher;
    stella::blob::Index index;
    std::vector<PackedAmp> amps;
    std::vector<PackedCab> cabs;
    std::vector<Entry> entries;
    bool open = false, locked = false;

    /** The pack's own lines read (Index::extra). "" when they make sense, else why not. */
    juce::String readLines()
    {
        amps.clear();
        cabs.clear();
        entries.clear();
        bool said = false;

        for (const auto& raw : index.extra)
        {
            const auto line = lineOf (raw);

            if (line.kind == "kind")
            {
                // kind <tab> axis <tab> 1: the fields have no key, so they're read from the text.
                const auto fields = juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (raw.c_str())), "\t", "");

                if (fields.size() < 2 || fields[1].trim() != indexKind)
                    return "It isn't an AXIS pack.";

                if (fields.size() > 2 && fields[2].getIntValue() > indexVersion)
                    return "It's an AXIS pack of a newer kind than this AXIS knows.";

                said = true;
                continue;
            }

            if (line.kind == "amp")
            {
                PackedAmp amp;
                amp.entry.kind = Entry::Kind::amp;
                amp.entry.index = (int) amps.size();
                amp.entry.name = line.text ("name");
                amp.entry.maker = line.text ("maker");
                amp.entry.gearMake = line.text ("make");
                amp.entry.gearModel = line.text ("model");
                amp.entry.toneType = line.text ("tone");
                amp.entry.sampleRate = line.number ("rate", captureRate);
                amp.entry.hasCab = line.integer ("cab", 0) != 0;
                amp.gearType = line.text ("gear");
                amp.namVersion = line.text ("nam");
                amp.loudness = line.number ("loudness", 0.0);
                amp.gain = line.number ("gain", 1.0);
                amp.esr = line.number ("esr", -1.0);
                amps.push_back (std::move (amp));
                continue;
            }

            if (line.kind == "sub")
            {
                const auto which = line.integer ("amp", -1);

                if (which < 0 || which >= (int) amps.size())
                    return "Its list of amps is damaged.";

                PackedSubmodel submodel;
                submodel.architecture = line.text ("arch", "WaveNet");
                submodel.maxValue = line.number ("max", 1.0);
                submodel.loudness = line.number ("loudness", 0.0);
                submodel.gain = line.number ("gain", 1.0);
                submodel.sample = line.integer ("sample", -1);
                submodel.weights = line.integer ("weights", 0);
                submodel.config = fromBase64 (line.text ("config"));

                if (submodel.sample < 0 || submodel.sample >= (int) index.samples.size() || submodel.config.isEmpty())
                    return "One of its captures is damaged.";

                amps[(size_t) which].submodels.push_back (std::move (submodel));
                continue;
            }

            if (line.kind == "cab")
            {
                PackedCab cab;
                cab.entry.kind = Entry::Kind::cab;
                cab.entry.index = (int) cabs.size();
                cab.entry.name = line.text ("name");
                cab.entry.maker = line.text ("maker");
                cab.entry.gearMake = line.text ("make");
                cab.entry.gearModel = line.text ("model");
                cab.entry.mic = line.text ("mic");
                cab.sample = line.integer ("sample", -1);

                if (cab.sample < 0 || cab.sample >= (int) index.samples.size())
                    return "One of its cabinets is damaged.";

                const auto& sample = index.samples[(size_t) cab.sample];
                cab.entry.sampleRate = sample.rate;
                cab.entry.frames = (int) sample.frames;
                cab.entry.channels = sample.channels > 1 ? 2 : 1;
                cabs.push_back (std::move (cab));
                continue;
            }
        }

        if (! said)
            return "It isn't an AXIS pack.";

        for (auto& amp : amps)
        {
            if (amp.submodels.empty())
                return "One of its captures has no model in it.";

            // Cheapest first, as a capture's widths always are.
            std::stable_sort (amp.submodels.begin(), amp.submodels.end(),
                              [] (const PackedSubmodel& a, const PackedSubmodel& b)
                              {
                                  if (a.maxValue != b.maxValue)
                                      return a.maxValue < b.maxValue;

                                  return a.weights < b.weights;
                              });

            amp.entry.submodels = (int) amp.submodels.size();
            entries.push_back (amp.entry);
        }

        for (const auto& cab : cabs)
            entries.push_back (cab.entry);

        return {};
    }

    /** One sample's frames, the file opened for it and closed after. */
    bool frames (int sample, std::vector<float>& left, std::vector<float>& right, juce::String& error) const
    {
        if (! open || sample < 0 || sample >= (int) index.samples.size())
        {
            error = "The pack isn't open.";
            return false;
        }

        const auto path = file.getFullPathName().toStdString();
        auto* handle = stella::blob::openFile (path, false);

        if (handle == nullptr)
        {
            error = file.getFileName() + " can't be read any more.";
            return false;
        }

        std::string why;
        const auto ok = stella::blob::readSample (handle, head, cipher, index.samples[(size_t) sample], left, right, why);
        std::fclose (handle);

        if (! ok)
            error = file.getFileName() + ": " + juce::String (why);

        return ok;
    }
};

//==============================================================================
Pack::Pack() : impl (std::make_unique<Impl>()) {}
Pack::~Pack() = default;

bool Pack::open (const juce::File& blob, juce::String& error)
{
    close();

    if (! blob.existsAsFile())
    {
        error = blob.getFileName() + " isn't there.";
        return false;
    }

    const auto path = blob.getFullPathName().toStdString();
    auto* handle = stella::blob::openFile (path, false);

    if (handle == nullptr)
    {
        error = blob.getFileName() + " can't be read.";
        return false;
    }

    std::string why;
    const auto ok = stella::blob::openForReading (handle, path, impl->head, impl->cipher, impl->index, why);
    std::fclose (handle);

    if (! ok)
    {
        error = blob.getFileName() + ": " + juce::String (why);
        close();
        return false;
    }

    impl->file = blob;
    impl->open = true;
    impl->locked = impl->head.keyKind != stella::blob::openKey;

    if (const auto problem = impl->readLines(); problem.isNotEmpty())
    {
        error = blob.getFileName() + ": " + problem;
        close();
        return false;
    }

    return true;
}

void Pack::close()
{
    impl = std::make_unique<Impl>();
}

bool Pack::isOpen() const noexcept                    { return impl->open; }
const juce::File& Pack::getFile() const noexcept      { return impl->file; }
bool Pack::isLocked() const noexcept                  { return impl->locked; }

juce::String Pack::getName() const
{
    const auto name = juce::String (juce::CharPointer_UTF8 (impl->index.name.c_str())).trim();
    return name.isNotEmpty() ? name : impl->file.getFileNameWithoutExtension();
}

juce::String Pack::getProduct() const
{
    return juce::String (juce::CharPointer_UTF8 (impl->head.product.c_str()));
}

const std::vector<Entry>& Pack::getEntries() const noexcept
{
    return impl->entries;
}

int Pack::count (Entry::Kind kind) const noexcept
{
    return (int) (kind == Entry::Kind::amp ? impl->amps.size() : impl->cabs.size());
}

bool Pack::loadCapture (int ampIndex, Capture& into, juce::String& error) const
{
    into = Capture();

    if (! impl->open || ampIndex < 0 || ampIndex >= (int) impl->amps.size())
    {
        error = "That capture isn't in the pack.";
        return false;
    }

    const auto& amp = impl->amps[(size_t) ampIndex];
    into.name = amp.entry.name;
    into.maker = amp.entry.maker;
    into.gearMake = amp.entry.gearMake;
    into.gearModel = amp.entry.gearModel;
    into.gearType = amp.gearType;
    into.toneType = amp.entry.toneType;
    into.namVersion = amp.namVersion;
    into.sampleRate = amp.entry.sampleRate;
    into.loudness = amp.loudness;
    into.gain = amp.gain;
    into.esr = amp.esr;
    into.hasCab = amp.entry.hasCab;

    for (const auto& packed : amp.submodels)
    {
        Capture::Submodel submodel;
        submodel.architecture = packed.architecture;
        submodel.maxValue = packed.maxValue;
        submodel.loudness = packed.loudness;
        submodel.gain = packed.gain;
        submodel.config = packed.config;

        std::vector<float> right;

        if (! impl->frames (packed.sample, submodel.weights, right, error))
            return false;

        if (packed.weights > 0 && (int) submodel.weights.size() != packed.weights)
        {
            error = into.displayName() + " is damaged: its model has " + juce::String ((int) submodel.weights.size())
                  + " weights where the pack says " + juce::String (packed.weights) + ".";
            return false;
        }

        into.submodels.push_back (std::move (submodel));
    }

    if (! into.isValid())
    {
        error = into.displayName() + " has no model AXIS can play.";
        return false;
    }

    return true;
}

bool Pack::loadIr (int cabIndex, Ir& into, juce::String& error) const
{
    into = Ir();

    if (! impl->open || cabIndex < 0 || cabIndex >= (int) impl->cabs.size())
    {
        error = "That cabinet isn't in the pack.";
        return false;
    }

    const auto& cab = impl->cabs[(size_t) cabIndex];
    into.name = cab.entry.name;
    into.maker = cab.entry.maker;
    into.gearMake = cab.entry.gearMake;
    into.gearModel = cab.entry.gearModel;
    into.mic = cab.entry.mic;
    into.sampleRate = cab.entry.sampleRate;

    if (! impl->frames (cab.sample, into.left, into.right, error))
        return false;

    if (! into.isValid())
    {
        error = into.displayName() + " has nothing in it.";
        return false;
    }

    return true;
}

bool Pack::looksLikePack (const juce::File& blob)
{
    Pack pack;
    juce::String error;
    return pack.open (blob, error);
}

//==============================================================================
juce::String write (const Contents& contents, const juce::File& blob,
                    const std::function<bool (double)>& progress,
                    const std::vector<std::uint8_t>& soni, juce::StringArray* notes)
{
    if (contents.isEmpty())
        return "There's nothing to pack.";

    // Protected: locked to the product, its key from the product's formula; its license.soni goes
    // beside it (one folder, one product), exactly as a sampler's blobs are (BlobMaker.cpp).
    const bool locked = ! soni.empty();
    const auto soniFile = blob.getSiblingFile (stella::licence::fileName);
    stella::licence::Soni product;
    std::uint8_t key[32] {};

    if (locked)
    {
        if (! Licences::readSoni (soni.data(), soni.size(), product) || ! product.isValid() || product.product.size() >= 64)
            return "The protection's license.soni can't be read: choose the protection again.";

        if (soniFile.existsAsFile())
        {
            stella::licence::Soni there;

            if (! Licences::readSoni (soniFile, there))
                return soniFile.getFullPathName() + " isn't a license.soni Sonify can read: move it away, or make the pack in another folder.";

            if (there.product != product.product)
                return "Its folder already has another product's license.soni: one folder, one product. Make the pack in another folder.";
        }

        stella::blob::licenceKey (product, key);
    }

    //==========================================================================
    // The index: the samples that hold the numbers, and the pack's own lines about them.
    stella::blob::Index index;
    index.name = contents.name.trim().isEmpty() ? std::string ("AXIS") : clean (contents.name).toStdString();

    struct Block
    {
        const std::vector<float>* left = nullptr;
        const std::vector<float>* right = nullptr;
    };

    std::vector<Block> blocks;
    juce::StringArray lines;
    lines.add ("kind\t" + juce::String (indexKind) + "\t" + juce::String (indexVersion));
    juce::int64 totalFrames = 0;

    const auto addSample = [&] (const juce::String& name, const std::vector<float>& left,
                                const std::vector<float>* right, double rate)
    {
        stella::blob::SampleEntry entry;
        entry.name = clean (name).toStdString();
        entry.frames = (juce::int64) left.size();
        entry.rate = rate >= 1000.0 && rate <= 768000.0 ? rate : captureRate;
        entry.channels = right != nullptr && ! right->empty() ? 2 : 1;
        entry.format = stella::blob::Format::f32;
        index.samples.push_back (entry);
        blocks.push_back ({ &left, right != nullptr && ! right->empty() ? right : nullptr });
        totalFrames += entry.frames;
        return (int) index.samples.size() - 1;
    };

    for (size_t a = 0; a < contents.captures.size(); ++a)
    {
        const auto& capture = contents.captures[a];

        if (! capture.isValid())
            return "One of the captures has no model AXIS can play.";

        lines.add ("amp"
                   + field ("name", capture.displayName())
                   + field ("maker", capture.maker)
                   + field ("make", capture.gearMake)
                   + field ("model", capture.gearModel)
                   + field ("gear", capture.gearType)
                   + field ("tone", capture.toneType)
                   + field ("nam", capture.namVersion)
                   + field ("rate", capture.sampleRate)
                   + field ("loudness", capture.loudness)
                   + field ("gain", capture.gain)
                   + field ("esr", capture.esr)
                   + field ("cab", capture.hasCab ? 1 : 0)
                   + field ("subs", (int) capture.submodels.size()));

        for (size_t s = 0; s < capture.submodels.size(); ++s)
        {
            const auto& submodel = capture.submodels[s];
            const auto sample = addSample (capture.displayName() + " (" + juce::String ((int) s + 1) + ")",
                                           submodel.weights, nullptr, capture.sampleRate);

            lines.add ("sub"
                       + field ("amp", (int) a)
                       + field ("sample", sample)
                       + field ("arch", submodel.architecture)
                       + field ("max", submodel.maxValue)
                       + field ("weights", (int) submodel.weights.size())
                       + field ("loudness", submodel.loudness)
                       + field ("gain", submodel.gain)
                       + field ("config", base64 (submodel.config)));
        }
    }

    for (const auto& ir : contents.irs)
    {
        if (! ir.isValid())
            return "One of the impulse responses has nothing in it.";

        const auto sample = addSample (ir.displayName(), ir.left, ir.right.empty() ? nullptr : &ir.right, ir.sampleRate);

        lines.add ("cab"
                   + field ("name", ir.displayName())
                   + field ("maker", ir.maker)
                   + field ("make", ir.gearMake)
                   + field ("model", ir.gearModel)
                   + field ("mic", ir.mic)
                   + field ("sample", sample));
    }

    for (const auto& line : lines)
        index.extra.push_back (line.toStdString());

    //==========================================================================
    // Written whole under another name first: a pack that's there is always complete.
    if (! blob.getParentDirectory().createDirectory())
        return "Couldn't make the folder " + blob.getParentDirectory().getFullPathName();

    const auto part = blob.getSiblingFile (blob.getFileName() + ".part");
    stella::blob::Writer writer;
    std::string error;

    if (! writer.open (part.getFullPathName().toStdString(), index, error,
                       locked ? key : nullptr, locked ? product.product : std::string()))
        return juce::String (error);

    juce::int64 done = 0;

    for (const auto& block : blocks)
    {
        if (block.left == nullptr || block.left->empty())
            continue;

        if (! writer.write (block.left->data(), block.right != nullptr ? block.right->data() : nullptr,
                            (int) block.left->size(), error))
        {
            writer.abandon();
            return juce::String (error);
        }

        done += (juce::int64) block.left->size();

        if (progress != nullptr && ! progress ((double) done / (double) juce::jmax ((juce::int64) 1, totalFrames)))
        {
            writer.abandon();
            return "Stopped.";
        }
    }

    if (! writer.finish (error))
    {
        part.deleteFile();
        return juce::String (error);
    }

    // A protected pack's license.soni beside it (left as it is when it's the same file).
    if (locked)
    {
        juce::MemoryBlock there;

        if (! (soniFile.existsAsFile() && soniFile.loadFileAsData (there) && there.matches (soni.data(), soni.size()))
            && ! soniFile.replaceWithData (soni.data(), soni.size()))
        {
            part.deleteFile();
            return "Couldn't write its license.soni (" + soniFile.getFullPathName() + "): without it, the pack can't be played.";
        }
    }

    if (blob.exists() && ! blob.deleteFile())
    {
        part.deleteFile();
        return "Couldn't replace " + blob.getFullPathName() + " (is it open somewhere?).";
    }

    if (! part.moveFileTo (blob))
    {
        part.deleteFile();
        return "Couldn't write " + blob.getFullPathName();
    }

    if (notes != nullptr)
    {
        int slimmable = 0, withCab = 0;

        for (const auto& capture : contents.captures)
        {
            if (capture.submodels.size() > 1)
                ++slimmable;

            if (capture.hasCab)
                ++withCab;
        }

        if (withCab > 0)
            notes->add (juce::String (withCab) + (withCab == 1 ? " capture has" : " captures have")
                        + " a cabinet in it already: AXIS leaves its own cabinet out for those");

        if (slimmable > 0)
            notes->add (juce::String (slimmable) + (slimmable == 1 ? " capture comes" : " captures come")
                        + " at more than one width: AXIS plays the widest");
    }

    return {};
}

}   // namespace AxisPack
