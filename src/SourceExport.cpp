// D:\workspace\Fanan_web_hub\src\SourceExport.cpp

#include "SourceExport.h"

#include "LayerDoc.h"
#include "Licences.h"
#include "PluginLibrary.h"
#include "PresetBank.h"
#include "ShapeDoc.h"
#include "TextureLibrary.h"
#include "WasmCompiler.h"

#include "SonifyFonts.h"
#include "SonifyIcons.h"
#include "SonifySourceKit.h"
#include "StellaLooks.h"
#include "StellaPrimitives.h"
#include "StellaRuntimeData.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <type_traits>

namespace
{
    //==========================================================================
    // C++ as text.

    /** A string as a C++ literal: its UTF-8 as it is (the build reads sources as UTF-8), quotes,
        backslashes and control characters escaped, in pieces of at most 2000 bytes (MSVC takes
        16380 a piece), split between characters. */
    juce::String quoted (const juce::String& value)
    {
        const auto* bytes = value.toRawUTF8();
        std::string out ("\"");
        int run = 0;

        for (const auto* c = bytes; *c != 0; ++c)
        {
            const auto byte = (std::uint8_t) *c;

            if (run >= 2000 && (byte & 0xc0) != 0x80)
            {
                out += "\" \"";
                run = 0;
            }

            if (*c == '"')                       out += "\\\"";
            else if (*c == '\\')               out += "\\\\";
            else if (*c == '\n')                out += "\\n";
            else if (*c == '\r')                out += "\\r";
            else if (*c == '\t')                out += "\\t";
            else if (*c == '?' && c[1] == '?')   out += "?\\";   // "??" would start a trigraph in older compilers
            else if (byte < 0x20)
            {
                char octal[8];
                std::snprintf (octal, sizeof (octal), "\\%03o", (unsigned) byte);
                out += octal;
            }
            else
            {
                out += *c;
            }

            ++run;
        }

        out += "\"";
        return juce::String::fromUTF8 (out.c_str(), (int) out.size());
    }

    /** utf8 ("..."): every text in the generated code (a name in any language). Over 60000
        bytes (a long SVG path): utf8 ({ "...", "..." }), MSVC's limit for one literal. */
    juce::String text (const juce::String& value)
    {
        if (value.getNumBytesAsUTF8() < 60000)
            return "utf8 (" + quoted (value) + ")";

        juce::String out ("utf8 ({ ");
        const auto* bytes = value.toRawUTF8();
        const auto size = (int) value.getNumBytesAsUTF8();

        for (int start = 0; start < size;)
        {
            auto end = juce::jmin (size, start + 30000);

            while (end < size && (((std::uint8_t) bytes[end]) & 0xc0) == 0x80)
                ++end;

            out << (start > 0 ? ", " : "") << quoted (juce::String::fromUTF8 (bytes + start, end - start));
            start = end;
        }

        return out + " })";
    }

    /** The shortest %g text that reads back bit-for-bit as the same value.
        Floating-point std::to_chars needs macOS 13.3, so this stays on snprintf/strtod. */
    template <typename T>
    std::string shortestDecimal (T value)
    {
        constexpr int maxDigits = std::numeric_limits<T>::max_digits10;
        char buffer[64] = {};

        for (int digits = 1; digits <= maxDigits; ++digits)
        {
            std::snprintf (buffer, sizeof (buffer), "%.*g", digits, (double) value);

            for (auto* c = buffer; *c != 0; ++c)
                if (*c == ',')
                    *c = '.';

            T back;

            if constexpr (std::is_same_v<T, float>)
                back = std::strtof (buffer, nullptr);
            else
                back = (T) std::strtod (buffer, nullptr);

            if (std::memcmp (&back, &value, sizeof (T)) == 0)
                break;
        }

        return buffer;
    }

    /** The shortest decimal that reads back as exactly this float, as a float literal. */
    juce::String floatLiteral (float value)
    {
        if (! std::isfinite (value))
            return value != value ? juce::String ("std::numeric_limits<float>::quiet_NaN()")
                                  : (value > 0 ? juce::String ("std::numeric_limits<float>::infinity()") : juce::String ("-std::numeric_limits<float>::infinity()"));

        juce::String s (shortestDecimal (value));

        if (! s.containsAnyOf (".e"))
            s << ".0";

        return s + "f";
    }

    /** ...and as a double. */
    juce::String doubleLiteral (double value)
    {
        if (! std::isfinite (value))
            return value != value ? juce::String ("std::numeric_limits<double>::quiet_NaN()")
                                  : (value > 0 ? juce::String ("std::numeric_limits<double>::infinity()") : juce::String ("-std::numeric_limits<double>::infinity()"));

        juce::String s (shortestDecimal (value));

        if (! s.containsAnyOf (".e"))
            s << ".0";

        return s;
    }

    juce::String colourLiteral (juce::Colour colour)
    {
        return "juce::Colour (0x" + juce::String::toHexString ((juce::int64) colour.getARGB()).paddedLeft ('0', 8) + "u)";
    }

    /** A value of any kind as C++: a number, a word, a list (list ({ ... })), an object
        (object ({ { "key", value }, ... })), each exactly as it is. */
    juce::String varLiteral (const juce::var& v, int indent)
    {
        if (v.isBool())     return static_cast<bool> (v) ? "juce::var (true)" : "juce::var (false)";
        if (v.isInt())      return "juce::var (" + juce::String (static_cast<int> (v)) + ")";
        if (v.isInt64())    return "juce::var ((juce::int64) " + juce::String (static_cast<juce::int64> (v)) + ")";
        if (v.isDouble())   return "juce::var (" + doubleLiteral (static_cast<double> (v)) + ")";
        if (v.isString())   return "juce::var (" + text (v.toString()) + ")";
        if (v.isVoid() || v.isUndefined())   return "juce::var()";

        const auto pad = juce::String::repeatedString (" ", indent + 4);

        if (const auto* array = v.getArray())
        {
            if (array->isEmpty())
                return "list ({})";

            // A list of plain numbers on one line; anything else one item a line.
            bool plain = true;

            for (const auto& item : *array)
                plain = plain && (item.isInt() || item.isDouble() || item.isBool() || item.isInt64());

            juce::String out ("list ({");

            for (int i = 0; i < array->size(); ++i)
                out << (plain ? (i == 0 ? " " : ", ") : (i == 0 ? "\n" + pad : ",\n" + pad)) << varLiteral (array->getReference (i), indent + 4);

            return out + (plain ? " })" : "\n" + juce::String::repeatedString (" ", indent) + "})");
        }

        if (auto* object = v.getDynamicObject())
        {
            const auto& properties = object->getProperties();

            if (properties.isEmpty())
                return "object ({})";

            juce::String out ("object ({");

            for (int i = 0; i < properties.size(); ++i)
                out << (i == 0 ? "\n" : ",\n") << pad << "{ " << text (properties.getName (i).toString()) << ", "
                    << varLiteral (properties.getValueAt (i), indent + 4) << " }";

            return out + "\n" + juce::String::repeatedString (" ", indent) + "})";
        }

        return "juce::var()";   // (a method or binary data: never in a layout)
    }

    /** Text for a comment line: line breaks and other control characters as spaces. */
    juce::String oneLine (const juce::String& value)
    {
        juce::String clean;

        for (auto c : value)
            clean << juce::String::charToString (c < 0x20 || c == 0x7f || c == 0x2028 || c == 0x2029 ? (juce::juce_wchar) ' ' : c);

        return clean;
    }

    juce::String typeLiteral (GuiWidget::Type type)
    {
        static const char* const names[] { "knob", "slider", "toggle", "selector", "label", "group", "meter", "scope", "lamp", "envelope",
                                           "filter", "xy", "shape", "preset", "image", "keyboard", "custom", "registration", "tabs", "matrix" };
        const auto index = (int) type;
        return juce::String ("GuiWidget::Type::") + (juce::isPositiveAndBelow (index, (int) std::size (names)) ? names[index] : "knob");
    }

    juce::String stringArrayLiteral (const juce::StringArray& strings)
    {
        juce::String out ("{ ");

        for (int i = 0; i < strings.size(); ++i)
            out << (i > 0 ? ", " : "") << text (strings[i]);

        return out + " }";
    }

    /** A line saying what an element is, for someone reading the window's code. */
    juce::String describe (const GuiWidget& w, int number)
    {
        juce::String line;
        line << number << ". " << GuiLayout::typeName (w.type);

        if (w.type == GuiWidget::Type::custom && w.element.isNotEmpty())
            line << " (" << w.element << ")";

        if (w.label.isNotEmpty())
            line << " \"" << w.label.substring (0, 60) << "\"";

        if (w.param.isNotEmpty())
            line << ", controls " << w.param;

        if (w.image.isNotEmpty())
            line << ", picture " << w.image;

        if (w.source.isNotEmpty())
            line << ", watches " << w.source;

        line << ": at " << w.bounds.getX() << ", " << w.bounds.getY() << ", " << w.bounds.getWidth() << " x " << w.bounds.getHeight();

        if (w.style.isNotEmpty())
            line << ", look \"" << w.style << "\"";

        line << (w.page.isNotEmpty() ? ", page \"" + w.page + "\"" : juce::String (", every page"));

        return oneLine (line);   // a comment line: no line breaks in it (a caption's own)
    }

    //==========================================================================
    // Ids, as Export makes them.
    std::uint32_t fnv (const juce::String& value)
    {
        std::uint32_t hash = 2166136261u;

        for (const auto* c = value.toRawUTF8(); *c != 0; ++c)
        {
            hash ^= (std::uint8_t) *c;
            hash *= 16777619u;
        }

        return hash;
    }

    /** Four letters and digits from a hash, the first a capital (what JUCE asks of its codes). */
    juce::String fourCharCode (std::uint32_t hash, juce::juce_wchar first)
    {
        static const char* const characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        juce::String code;
        code << juce::String::charToString (first);

        while (code.length() < 4)
        {
            code << juce::String::charToString ((juce::juce_wchar) characters[hash % 62]);
            hash = hash / 62 + 0x9e3779b9u * (std::uint32_t) code.length();
        }

        return code;
    }

    juce::juce_wchar capitalOf (const juce::String& value, juce::juce_wchar fallback)
    {
        const auto letters = value.toUpperCase().retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        return letters.isNotEmpty() ? letters[0] : fallback;
    }

    /** A name CMake and the compiler take: letters and digits, starting with a letter. */
    juce::String targetName (const juce::String& name)
    {
        juce::String out;
        bool capital = true;

        for (auto c : name)
        {
            if (c < 128 && juce::CharacterFunctions::isLetterOrDigit (c))
            {
                out << juce::String::charToString (capital ? juce::CharacterFunctions::toUpperCase (c) : c);
                capital = false;
            }
            else
            {
                capital = true;
            }
        }

        if (out.isEmpty() || ! juce::CharacterFunctions::isLetter (out[0]))
            out = "Plugin" + out;

        return out;
    }

    /** CMake's VERSION: numbers only, x.y.z. */
    juce::String cmakeVersion (const juce::String& version)
    {
        auto parts = juce::StringArray::fromTokens (version, ".", "");
        juce::StringArray numbers;

        for (int i = 0; i < 3; ++i)
            numbers.add (juce::String (juce::jmax (0, parts[i].getIntValue())));

        return numbers.joinIntoString (".");
    }

    juce::String cmakeQuoted (const juce::String& value)
    {
        return "\"" + value.replace ("\\", "/").replace ("\"", "\\\"").replace ("$", "\\$") + "\"";
    }

    //==========================================================================
    /** What the export writes: every file by its path in source\, written only when it changed;
        what was there before and isn't any more is taken away (in the folders Sonify keeps). */
    class Writer
    {
    public:
        explicit Writer (const juce::File& rootFolder) : root (rootFolder) {}

        bool put (const juce::String& path, const void* data, size_t size)
        {
            const auto file = root.getChildFile (path);
            written.insert (file.getFullPathName());

            if (file.existsAsFile() && file.getSize() == (juce::int64) size)
            {
                juce::MemoryBlock existing;

                if (file.loadFileAsData (existing) && existing.getSize() == size && (size == 0 || std::memcmp (existing.getData(), data, size) == 0))
                    return true;
            }

            ++changedFiles;
            return file.getParentDirectory().createDirectory().wasOk() && file.replaceWithData (data, size);
        }

        bool put (const juce::String& path, const juce::MemoryBlock& data)    { return put (path, data.getData(), data.getSize()); }

        bool putText (const juce::String& path, const juce::String& textToWrite)
        {
            return put (path, textToWrite.toRawUTF8(), textToWrite.getNumBytesAsUTF8());
        }

        /** Source code from Sonify: its first line (where it lives on Sonify's maker's computer)
            becomes where it is in source\. */
        bool putSource (const juce::String& path, const char* data, int size)
        {
            auto content = juce::String::fromUTF8 (data, size);

            if (content.startsWith ("// D:\\") || content.startsWith ("// C:\\"))
                content = "// " + path + content.fromFirstOccurrenceOf ("\n", true, false);

            return putText (path, content);
        }

        bool putFile (const juce::String& path, const juce::File& source)
        {
            juce::MemoryBlock data;
            return source.loadFileAsData (data) && put (path, data);
        }

        /** Takes away what's left from before in the folders Sonify keeps (never anything else). */
        void removeStale (const juce::StringArray& managedFolders, const juce::StringArray& managedFiles)
        {
            for (const auto& managed : managedFolders)
            {
                const auto folder = root.getChildFile (managed);

                for (const auto& file : folder.findChildFiles (juce::File::findFiles, true))
                    if (written.count (file.getFullPathName()) == 0)
                        file.deleteFile();

                // Folders emptied by that.
                for (const auto& sub : folder.findChildFiles (juce::File::findDirectories, true))
                    if (sub.isDirectory() && sub.findChildFiles (juce::File::findFilesAndDirectories, true).isEmpty())
                        sub.deleteRecursively();
            }

            for (const auto& name : managedFiles)
                if (const auto file = root.getChildFile (name); file.existsAsFile() && written.count (file.getFullPathName()) == 0)
                    file.deleteFile();
        }

        juce::StringArray filesIn (const juce::String& folder) const
        {
            juce::StringArray found;
            const auto base = root.getChildFile (folder);

            for (const auto& path : written)
                if (const juce::File file (path); file.isAChildOf (base))
                    found.add (file.getRelativePathFrom (root).replaceCharacter ('\\', '/'));

            found.sort (true);
            return found;
        }

        int changedFiles = 0;

    private:
        juce::File root;
        std::set<juce::String> written;
    };

    /** Binary data, each file by its own name (juce_add_binary_data's namespaces). */
    template <typename Visit>
    void forEachResource (const char* const* names, int count, const char* (*data) (const char*, int&),
                          const char* (*original) (const char*), Visit&& visit)
    {
        for (int i = 0; i < count; ++i)
        {
            int size = 0;

            if (const auto* bytes = data (names[i], size); bytes != nullptr)
            {
                const auto file = juce::String::fromUTF8 (original (names[i])).replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false);
                visit (file, bytes, size);
            }
        }
    }

    bool isSourceFile (const juce::String& name)
    {
        return name.endsWith (".h") || name.endsWith (".cpp") || name.endsWith (".mm") || name.endsWith (".c");
    }

    //==========================================================================
    // A look's pictures, shapes and textures: what it draws from outside itself.
    void gatherLookNeeds (const LayerDoc& doc, SourceExport::Request& request, bool& pinnedPicture)
    {
        for (const auto& layer : doc.layers)
        {
            const auto& p = layer.prim;

            if (p.type == PrimType::Image && juce::File::isAbsolutePath (p.imageFile.trim()))
            {
                pinnedPicture = true;

                if (const juce::File picture (p.imageFile.trim()); picture.existsAsFile())
                    request.pictures.addIfNotAlreadyThere (picture);
            }

            if (p.shapeName.isNotEmpty())
                if (const auto shape = ShapeLibrary::folder().getChildFile (juce::File::createLegalFileName (p.shapeName) + ".fkshape"); shape.existsAsFile())
                    request.shapes.addIfNotAlreadyThere (shape);

            if (p.textureMode != TextureMode::Off && p.textureName.isNotEmpty() && ! TextureLibrary::isProcedural (p.textureName))
                if (const auto texture = TextureLibrary::userFolder().getChildFile (p.textureName); texture.existsAsFile())
                    request.textures.addIfNotAlreadyThere (texture);
        }
    }

    /** A look file as the plugin carries it: as it is, or with the pictures it names by a full
        path named by their file's name instead (copied into its pictures). */
    juce::MemoryBlock lookFileFor (const juce::File& file, const Looks::Look& look, bool pinned)
    {
        if (! pinned)
        {
            juce::MemoryBlock data;
            file.loadFileAsData (data);
            return data;
        }

        auto copy = look;

        for (auto& layer : copy.doc.layers)
            if (layer.prim.type == PrimType::Image && juce::File::isAbsolutePath (layer.prim.imageFile.trim()))
                layer.prim.imageFile = juce::File (layer.prim.imageFile.trim()).getFileName();

        const auto temp = juce::File::createTempFile (Looks::fileExtension);
        juce::MemoryBlock data;

        if (Looks::writeFile (temp, copy))
            temp.loadFileAsData (data);

        temp.deleteFile();
        return data;
    }
}

//==============================================================================
SourceExport::Request SourceExport::prepare (const juce::File& projectFolder, const ProjectInfo& info, const GuiLayout& layout,
                                             Looks& looks, std::vector<juce::Image> pages)
{
    Request request;
    request.projectFolder = projectFolder;
    request.info = info;
    request.layout = layout;
    request.pages = std::move (pages);

    // The plugin's own looks: every one in gui\looks.
    for (const auto& file : projectFolder.getChildFile ("gui").getChildFile (Looks::folderName)
                                         .findChildFiles (juce::File::findFiles, false, juce::String ("*") + Looks::fileExtension))
    {
        Looks::Look look;

        if (Looks::readFile (file, look))
        {
            bool pinned = false;
            gatherLookNeeds (look.doc, request, pinned);
            request.ownLooks.push_back ({ file.getFileName(), lookFileFor (file, look, pinned) });
        }
    }

    // The maker's own looks it wears: each control's look as it's drawn (a close name finds
    // one too), and any named in the layout (a selector's keys, a preset manager's...).
    std::set<juce::String> wanted;
    const auto layoutText = juce::JSON::toString (layout.toVar(), true);

    for (const auto& w : layout.widgets)
        if (Looks::takesLook (w))
            if (const auto& look = looks.lookFor (w); look.origin == Looks::Origin::mine)
                wanted.insert (look.name);

    for (const auto& file : Looks::myLooksFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + Looks::fileExtension))
    {
        Looks::Look look;

        if (! Looks::readFile (file, look))
            continue;

        if (wanted.count (look.name) == 0 && ! layoutText.contains ("\"" + look.name + "\""))
            continue;

        bool pinned = false;
        gatherLookNeeds (look.doc, request, pinned);
        request.myLooks.push_back ({ file.getFileName(), lookFileFor (file, look, pinned) });
    }

    // What the built-in looks draw from (they ship with their shapes and textures, but a
    // project may have put its own over them by name).
    for (const auto* look : looks.all())
        if (look != nullptr && look->origin == Looks::Origin::builtIn)
        {
            bool pinned = false;
            gatherLookNeeds (look->doc, request, pinned);
        }

    return request;
}

//==============================================================================
juce::String SourceExport::windowSource (const GuiLayout& layout, const juce::String& pluginName)
{
    juce::MemoryOutputStream out;
    const GuiWidget standard;   // what an element is unless it says otherwise

    out << "// plugin/PluginWindow.cpp\n"
        << "//\n"
        << "// " << oneLine (pluginName) << "'s window, element by element: written by Sonify's Plugins Creator Studio from the\n"
        << "// project's gui/layout.json on every Save (the window as designed in its Edit UI). Change the\n"
        << "// window there; this file is written again.\n"
        << "//\n"
        << "// The window is " << layout.width << " x " << layout.height << " pixels";

    if (! layout.pages.isEmpty())
        out << ", in " << layout.pages.size() << " pages (" << layout.pages.joinIntoString (", ") << ")";

    out << ", with " << (int) layout.widgets.size() << " elements on it,\n"
        << "// listed back to front (the first is behind everything). An element says only what differs from\n"
        << "// a new GuiWidget's (GuiLayout.h says what each setting is); preview/ has pictures of it.\n\n"
        << "#include \"PluginDesign.h\"\n\n"
        << "#include <initializer_list>\n#include <limits>\n#include <string>\n#include <utility>\n\n"
        << "namespace\n{\n"
        << "    juce::String utf8 (const char* textInUtf8)    { return juce::String::fromUTF8 (textInUtf8); }\n\n"
        << "    juce::String utf8 (std::initializer_list<const char*> pieces)   // a text too long for one literal\n"
        << "    {\n"
        << "        std::string joined;\n\n"
        << "        for (const auto* piece : pieces)\n"
        << "            joined += piece;\n\n"
        << "        return juce::String::fromUTF8 (joined.c_str(), (int) joined.size());\n"
        << "    }\n\n"
        << "    juce::var list (std::initializer_list<juce::var> items)\n"
        << "    {\n"
        << "        juce::Array<juce::var> array;\n\n"
        << "        for (const auto& item : items)\n"
        << "            array.add (item);\n\n"
        << "        return array;\n"
        << "    }\n\n"
        << "    juce::var object (std::initializer_list<std::pair<juce::String, juce::var>> properties)\n"
        << "    {\n"
        << "        juce::DynamicObject::Ptr result (new juce::DynamicObject());\n\n"
        << "        for (const auto& [name, value] : properties)\n"
        << "            result->setProperty (name, value);\n\n"
        << "        return juce::var (result.get());\n"
        << "    }\n"
        << "}\n\n"
        << "GuiLayout PluginDesign::window()\n{\n"
        << "    GuiLayout layout;\n\n"
        << "    // The window: its size, its global light (where it comes from, how high), the panel camera,\n"
        << "    // the background's colours (top to bottom)";

    if (layout.body.isObject())
        out << ", its 3D body";

    out << (layout.pages.isEmpty() ? juce::String() : juce::String (", its pages")) << ".\n"
        << "    layout.width = " << layout.width << ";\n"
        << "    layout.height = " << layout.height << ";\n"
        << "    layout.lightAngle = " << floatLiteral (layout.lightAngle) << ";\n"
        << "    layout.lightAltitude = " << floatLiteral (layout.lightAltitude) << ";\n"
        << "    layout.camera = " << floatLiteral (layout.camera) << ";\n"
        << "    layout.backgroundTop = " << colourLiteral (layout.backgroundTop) << ";\n"
        << "    layout.backgroundBottom = " << colourLiteral (layout.backgroundBottom) << ";\n";

    if (layout.backgroundImage.isNotEmpty())
        out << "    layout.backgroundImage = " << text (layout.backgroundImage) << ";\n";

    if (layout.backgroundMode != GuiLayout().backgroundMode)
        out << "    layout.backgroundMode = " << text (layout.backgroundMode) << ";\n";

    if (! layout.pages.isEmpty())
        out << "    layout.pages = " << stringArrayLiteral (layout.pages) << ";\n";

    if (! layout.body.isVoid())
        out << "    layout.body = " << varLiteral (layout.body, 4) << ";\n";

    out << "\n    // The elements, back to front.\n";

    for (size_t i = 0; i < layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[i];

        out << "\n    // " << describe (w, (int) i + 1) << "\n"
            << "    {\n"
            << "        GuiWidget w;\n"
            << "        w.type = " << typeLiteral (w.type) << ";\n"
            << "        w.bounds = { " << w.bounds.getX() << ", " << w.bounds.getY() << ", " << w.bounds.getWidth() << ", " << w.bounds.getHeight() << " };\n";

        auto field = [&] (const char* name, const juce::String& value) { out << "        w." << name << " = " << value << ";\n"; };

        if (w.param != standard.param)                     field ("param", text (w.param));
        if (w.label != standard.label)                     field ("label", text (w.label));
        if (w.style != standard.style)                     field ("style", text (w.style));
        if (w.originalStyle != standard.originalStyle)     field ("originalStyle", text (w.originalStyle));
        if (w.originalKnown != standard.originalKnown)     field ("originalKnown", w.originalKnown ? "true" : "false");
        if (w.colour != standard.colour)                   field ("colour", colourLiteral (w.colour));
        if (w.fontSize != standard.fontSize)               field ("fontSize", floatLiteral (w.fontSize));
        if (w.bold != standard.bold)                       field ("bold", w.bold ? "true" : "false");
        if (w.italic != standard.italic)                   field ("italic", w.italic ? "true" : "false");
        if (w.underline != standard.underline)             field ("underline", w.underline ? "true" : "false");
        if (w.textColour != standard.textColour)           field ("textColour", colourLiteral (w.textColour));
        if (w.textShift != standard.textShift)             field ("textShift", juce::String (w.textShift));
        if (w.font != standard.font)                       field ("font", text (w.font));
        if (w.rounding != standard.rounding)               field ("rounding", floatLiteral (w.rounding));
        if (w.vertical != standard.vertical)               field ("vertical", w.vertical ? "true" : "false");
        if (w.options != standard.options)                 field ("options", stringArrayLiteral (w.options));
        if (w.source != standard.source)                   field ("source", text (w.source));
        if (w.mode != standard.mode)                       field ("mode", text (w.mode));
        if (w.image != standard.image)                     field ("image", text (w.image));
        if (w.threshold != standard.threshold)             field ("threshold", floatLiteral (w.threshold));
        if (w.path != standard.path)                       field ("path", text (w.path));
        if (w.stroke != standard.stroke)                   field ("stroke", colourLiteral (w.stroke));
        if (w.strokeWidth != standard.strokeWidth)         field ("strokeWidth", floatLiteral (w.strokeWidth));
        if (w.lowNote != standard.lowNote)                 field ("lowNote", juce::String (w.lowNote));
        if (w.highNote != standard.highNote)               field ("highNote", juce::String (w.highNote));
        if (w.element != standard.element)                 field ("element", text (w.element));
        if (w.angle != standard.angle)                     field ("angle", floatLiteral (w.angle));
        if (w.opacity != standard.opacity)                 field ("opacity", floatLiteral (w.opacity));
        if (w.page != standard.page)                       field ("page", text (w.page));
        if (w.command != standard.command)                 field ("command", text (w.command));

        for (const auto& [role, id] : w.roles)
            out << "        w.roles[" << text (role) << "] = " << text (id) << ";\n";

        for (const auto& [role, watched] : w.sources)
            out << "        w.sources[" << text (role) << "] = " << text (watched) << ";\n";

        for (const auto& [key, value] : w.settings)
            out << "        w.settings[" << text (key) << "] = " << varLiteral (value, 8) << ";\n";

        out << "        layout.widgets.push_back (std::move (w));\n"
            << "    }\n";
    }

    out << "\n    return layout;\n}\n";
    return out.toString();
}

//==============================================================================
namespace
{
    juce::String presetsSource (const std::vector<Preset>& presets, const juce::String& pluginName)
    {
        juce::MemoryOutputStream out;
        out << "// plugin/PluginPresets.cpp\n"
            << "//\n"
            << "// " << oneLine (pluginName) << "'s factory presets: written by Sonify's Plugins Creator Studio from the project's\n"
            << "// presets.json on every Save. Each preset sets parameters by their ids (\"<module>.<param>\");\n"
            << "// a parameter it doesn't name goes to its default.\n\n"
            << "#include \"PluginDesign.h\"\n\n"
            << "namespace\n{\n"
            << "    juce::String utf8 (const char* textInUtf8)    { return juce::String::fromUTF8 (textInUtf8); }\n"
            << "}\n\n"
            << "std::vector<Preset> PluginDesign::factoryPresets()\n{\n"
            << "    std::vector<Preset> presets;\n";

        for (const auto& preset : presets)
        {
            out << "\n    {\n"
                << "        Preset p;\n"
                << "        p.name = " << text (preset.name) << ";\n";

            for (const auto& [id, value] : preset.values)
                out << "        p.values[" << text (id) << "] = " << floatLiteral (value) << ";\n";

            out << "        presets.push_back (std::move (p));\n"
                << "    }\n";
        }

        out << "\n    return presets;\n}\n";
        return out.toString();
    }

    /** One folder of what the plugin carries: where it is in source\, its binary data's name,
        and where the plugin unpacks it. */
    struct AssetFolder
    {
        const char* source;      // in source\assets
        const char* target;      // juce_add_binary_data's name (its namespace)
        const char* unpackedTo;  // in the plugin's private folder
    };

    constexpr AssetFolder assetFolders[]
    {
        { "images",   "PluginImages",   "gui/images" },
        { "looks",    "PluginLooks",    "gui/looks" },
        { "mylooks",  "PluginMyLooks",  "data/Looks" },
        { "shapes",   "PluginShapes",   "data/Shapes" },
        { "textures", "PluginTextures", "data/Textures" },
    };

    juce::String assetsSource (const juce::StringArray& present)
    {
        juce::MemoryOutputStream out;
        out << "// plugin/PluginAssets.cpp\n"
            << "//\n"
            << "// What the plugin carries (the assets folder), as CMake built it in: written by Sonify's Plugins\n"
            << "// Creator Studio on every Save. PluginAssets.h unpacks it as the plugin starts.\n\n"
            << "#include \"PluginDesign.h\"\n";

        for (const auto& folder : assetFolders)
            if (present.contains (folder.source))
                out << "#include \"" << folder.target << ".h\"\n";

        out << "\nstd::vector<PluginDesign::AssetFolder> PluginDesign::assetFolders()\n{\n    return {\n";

        for (const auto& folder : assetFolders)
            if (present.contains (folder.source))
                out << "        { \"" << folder.unpackedTo << "\", " << folder.target << "::namedResourceList, " << folder.target << "::namedResourceListSize,\n"
                    << "          " << folder.target << "::getNamedResource, " << folder.target << "::getNamedResourceOriginalFilename },   // assets/" << folder.source << "\n";

        out << "    };\n}\n";
        return out.toString();
    }

    juce::String infoSource (const ProjectInfo& info, const juce::Array<PluginLibrary::Instrument>& library, bool isProtected)
    {
        const auto kind = info.kind == PluginKind::effect ? 1 : (info.kind == PluginKind::widget ? 2 : 0);

        juce::MemoryOutputStream out;
        out << "// plugin/PluginInfo.h\n"
            << "//\n"
            << "// What the plugin is: written by Sonify's Plugins Creator Studio from the project on every Save.\n\n"
            << "#pragma once\n\n"
            << "namespace PluginInfo\n{\n"
            << "    constexpr const char* name = " << quoted (info.name) << ";\n"
            << "    constexpr const char* vendor = " << quoted (info.vendor) << ";\n"
            << "    constexpr const char* id = " << quoted (info.pluginId) << ";\n"
            << "    constexpr const char* version = " << quoted (info.version) << ";\n"
            << "    constexpr int kind = " << kind << ";   // 0 an instrument, 1 an effect, 2 a widget (audio and notes in)\n"
            << "    constexpr bool isProtected = " << (isProtected ? "true" : "false") << ";   // plays its demo until it's registered (stella/stella_product.h)\n\n"
            << "    // A sampler's library: each instrument's name and its blob's file name, found on the computer\n"
            << "    // as the plugin starts (stella_blob.h's findPluginBlob says where it looks).\n"
            << "    struct LibraryInstrument { const char* name; const char* file; };\n"
            << "    inline constexpr LibraryInstrument library[] {";

        for (const auto& instrument : library)
            out << " { " << quoted (instrument.name) << ", " << quoted (instrument.file.getFileName()) << " },";

        if (library.isEmpty())
            out << " { \"\", \"\" }";

        out << " };\n"
            << "    constexpr int numLibrary = " << library.size() << ";\n"
            << "}\n";

        return out.toString();
    }

    juce::String productSource (const Licences::ProjectProtection& protection)
    {
        auto seconds = [] (double value)
        {
            return "(" + juce::String (juce::roundToInt (juce::jlimit (0.0, 3600.0, value) * 1000.0)) + " / 1000.0)";
        };

        return "// stella/stella_product.h\n//\n// The protected product the plugin is: written by Sonify's Plugins Creator Studio on every Save.\n\n"
               "#define STELLA_PRODUCT " + quoted (protection.product) + "\n"
               "#define STELLA_DEMO_PLAY " + seconds (protection.playSeconds) + "      // seconds of sound\n"
               "#define STELLA_DEMO_SILENT " + seconds (protection.silentSeconds) + "    // seconds of silence\n";
    }

    juce::String cmakeList (const juce::StringArray& files, int indent = 4)
    {
        juce::String out;

        for (const auto& file : files)
            out << juce::String::repeatedString (" ", indent) << "\"${CMAKE_CURRENT_SOURCE_DIR}/" << cmakeQuoted (file).unquoted() << "\"\n";

        return out;
    }

    struct CMakeFacts
    {
        ProjectInfo info;
        bool isProtected = false;
        juce::StringArray pluginSources, guiSources, stellaSources, moduleSources, elementSources;
        juce::StringArray registeredSources;           // the toolbox's programmed elements (stella_builtin_*.cpp)
        juce::StringArray kitData[5];                  // looks, primitives, fonts, icons, inter
        std::vector<std::pair<juce::String, juce::StringArray>> assets;   // binary data target, files
    };

    juce::String cmakeSource (const CMakeFacts& facts)
    {
        const auto& info = facts.info;
        const auto target = targetName (info.name);
        const auto kind = info.kind == PluginKind::effect ? 1 : (info.kind == PluginKind::widget ? 2 : 0);
        const auto vendor = info.vendor.isNotEmpty() ? info.vendor : juce::String ("Made in Sonify");
        const auto manufacturerCode = fourCharCode (fnv (vendor.toLowerCase()), capitalOf (vendor, 'S'));
        const auto pluginCode = fourCharCode (fnv (info.pluginId.isNotEmpty() ? info.pluginId : info.uuid), capitalOf (info.name, 'P'));
        auto bundleId = info.pluginId.isNotEmpty() ? info.pluginId : "com.sonify." + target.toLowerCase();
        bundleId = bundleId.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-");

        juce::MemoryOutputStream out;
        out << "# CMakeLists.txt\n"
            << "#\n"
            << "# " << oneLine (info.name) << (info.vendor.isNotEmpty() ? " by " + oneLine (info.vendor) : juce::String()) << ", version " << oneLine (info.version)
            << ": its whole source, written by Sonify's Plugins Creator Studio\n"
            << "# on every Save. It builds the plugin as it is in Sonify, the same sound and the same window, with\n"
            << "# JUCE 9: VST3, CLAP and Standalone (and an Audio Unit on a Mac). Everything is built in the build folder.\n"
            << "#\n"
            << "#     cmake -B build -DJUCE_DIR=<your JUCE 9 folder>\n"
            << "#     cmake --build build --config Release\n"
            << "#\n"
            << "# Without JUCE_DIR, JUCE 9.0.1 is downloaded into the build folder (git needed), and so is\n"
            << "# clap-juce-extensions for the CLAP (CLAP_JUCE_EXTENSIONS_DIR: yours; -DPLUGIN_CLAP=OFF: no CLAP).\n"
            << "# The plugins are in build/" << target << "_artefacts/Release (Debug: a Debug build).\n"
            << "#\n"
            << "# What's where:\n"
            << "#     plugin/     the plugin: PluginProcessor (the Stella runtime as JUCE's plugin), PluginEditor (its\n"
            << "#                 window), and what's this plugin's own: PluginInfo.h, PluginWindow.cpp (the window,\n"
            << "#                 element by element), PluginPresets.cpp, PluginAssets.cpp\n"
            << "#     gui/        Sonify's window code, which draws the window as Sonify does; gui/data: its built-in\n"
            << "#                 looks, panel fonts, icons and toolbox primitives\n"
            << "#     stella/     the Stella runtime that plays the modules, the graph (stella_graph.cpp), the\n"
            << "#                 programmed elements' runtime\n"
            << "#     modules/    the plugin's DSP modules\n"
            << "#     elements/   its programmed elements\n"
            << "#     assets/     what it carries: pictures, looks, the shapes and textures they draw from\n"
            << "#     preview/    pictures of the window, page by page\n"
            << "#\n"
            << "# Licences: JUCE's own (juce.com); Clipper2 (gui/clipper2_license.txt); the fonts and icons\n"
            << "# (gui/data/fonts/fonts_license.txt, gui/data/icons/icons_license.txt, gui/data/inter/inter_license.txt).\n\n"
            << "cmake_minimum_required(VERSION 3.22)\n\n"
            << "project(" << target << " VERSION " << cmakeVersion (info.version) << " LANGUAGES C CXX)\n\n"
            << "if(APPLE)\n"
            << "    enable_language(OBJC OBJCXX)   # JUCE's Mac sides\n"
            << "endif()\n\n"
            << "set(CMAKE_CXX_STANDARD 20)\n"
            << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
            << "# ------------------------------------------------------------------------------\n"
            << "# JUCE 9: yours (JUCE_DIR), or 9.0.1 downloaded\n"
            << "# ------------------------------------------------------------------------------\n"
            << "set(JUCE_DIR \"\" CACHE PATH \"JUCE 9's folder (empty: JUCE 9.0.1 is downloaded)\")\n\n"
            << "if(JUCE_DIR)\n"
            << "    if(NOT EXISTS \"${JUCE_DIR}/CMakeLists.txt\")\n"
            << "        message(FATAL_ERROR \"JUCE isn't at ${JUCE_DIR}\")\n"
            << "    endif()\n\n"
            << "    add_subdirectory(\"${JUCE_DIR}\" JUCE)\n"
            << "else()\n"
            << "    include(FetchContent)\n"
            << "    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.1 GIT_SHALLOW TRUE)\n"
            << "    FetchContent_MakeAvailable(JUCE)\n"
            << "endif()\n\n"
            << "# ------------------------------------------------------------------------------\n"
            << "# The plugin\n"
            << "# ------------------------------------------------------------------------------\n"
            << "set(PLUGIN_FORMATS VST3 Standalone)\n\n"
            << "if(APPLE)\n"
            << "    list(APPEND PLUGIN_FORMATS AU)\n"
            << "endif()\n\n"
            << "juce_add_plugin(" << target << "\n"
            << "    PRODUCT_NAME " << cmakeQuoted (info.name) << "\n"
            << "    COMPANY_NAME " << cmakeQuoted (vendor) << "\n"
            << "    BUNDLE_ID " << cmakeQuoted (bundleId) << "\n"
            << "    VERSION " << cmakeVersion (info.version) << "\n"
            << "    PLUGIN_MANUFACTURER_CODE " << manufacturerCode << "\n"
            << "    PLUGIN_CODE " << pluginCode << "\n"
            << "    IS_SYNTH " << (kind == 0 ? "TRUE" : "FALSE") << "\n"
            << "    NEEDS_MIDI_INPUT TRUE\n"
            << "    NEEDS_MIDI_OUTPUT FALSE\n"
            << "    IS_MIDI_EFFECT FALSE\n"
            << "    EDITOR_WANTS_KEYBOARD_FOCUS TRUE\n"
            << "    COPY_PLUGIN_AFTER_BUILD FALSE\n"
            << "    FORMATS ${PLUGIN_FORMATS})\n\n"
            << "target_sources(" << target << " PRIVATE\n"
            << "    # The plugin\n" << cmakeList (facts.pluginSources)
            << "\n    # Sonify's window code\n" << cmakeList (facts.guiSources)
            << "\n    # The Stella runtime, the graph, the programmed elements' runtime\n" << cmakeList (facts.stellaSources) << ")\n\n"
            << "target_include_directories(" << target << " PRIVATE\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/plugin\"\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/gui\"\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/stella\"\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/modules\"\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/elements\")\n\n"
            << "target_compile_definitions(" << target << " PRIVATE\n"
            << "    JUCE_WEB_BROWSER=0\n"
            << "    JUCE_USE_CURL=0\n"
            << "    JUCE_VST3_CAN_REPLACE_VST2=0\n"
            << "    STELLA_LICENCE_NATIVE=1          # the computer's side of protection (a protected sample library's too)\n"
            << "    STELLA_PROTECTED=" << (facts.isProtected ? 1 : 0) << ")\n\n"
            << "# The runtime and Clipper2 are plain C++ written for clang: read as UTF-8 on MSVC, their\n"
            << "# warnings left out of the build's.\n"
            << "set(PLUGIN_QUIET\n" << cmakeList (facts.stellaSources);

        for (const auto& file : facts.guiSources)
            if (file.contains ("clipper."))
                out << "    \"${CMAKE_CURRENT_SOURCE_DIR}/" << cmakeQuoted (file).unquoted() << "\"\n";

        out << ")\n\n"
            << "if(MSVC)\n"
            << "    target_compile_options(" << target << " PRIVATE /utf-8 /bigobj)\n"
            << "    set_source_files_properties(${PLUGIN_QUIET} PROPERTIES COMPILE_OPTIONS \"/W0\")\n"
            << "else()\n"
            << "    set_source_files_properties(${PLUGIN_QUIET} PROPERTIES COMPILE_OPTIONS \"-w\")\n"
            << "endif()\n\n"
            << "# What registers itself as the plugin loads (each DSP module type, each programmed element): its\n"
            << "# objects linked straight into every format (below), so no linker leaves them out of the plugin's\n"
            << "# static library as unused. Plain C++ like the runtime.\n"
            << "add_library(" << target << "_Registered OBJECT\n";

        if (! facts.moduleSources.isEmpty())
            out << "    # The DSP modules\n" << cmakeList (facts.moduleSources);

        out << "    # The programmed elements: the toolbox's, then the plugin's own\n" << cmakeList (facts.registeredSources);

        if (! facts.elementSources.isEmpty())
            out << cmakeList (facts.elementSources);

        out << ")\n\n"
            << "set_target_properties(" << target << "_Registered PROPERTIES POSITION_INDEPENDENT_CODE ON)\n"
            << "target_include_directories(" << target << "_Registered PRIVATE\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/stella\"\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/modules\"\n"
            << "    \"${CMAKE_CURRENT_SOURCE_DIR}/elements\")\n"
            << "target_compile_definitions(" << target << "_Registered PRIVATE STELLA_LICENCE_NATIVE=1 STELLA_PROTECTED=" << (facts.isProtected ? 1 : 0) << ")\n\n"
            << "if(MSVC)\n"
            << "    target_compile_options(" << target << "_Registered PRIVATE /utf-8 /W0)\n"
            << "else()\n"
            << "    target_compile_options(" << target << "_Registered PRIVATE -w)\n"
            << "endif()\n\n"
            << "# ------------------------------------------------------------------------------\n"
            << "# Built in: the window code's data (gui/data), and what the plugin carries (assets)\n"
            << "# ------------------------------------------------------------------------------\n";

        static const char* const kitTargets[] { "StellaLooks", "StellaPrimitives", "SonifyFonts", "SonifyIcons", "StellaRuntimeData" };
        juce::StringArray dataTargets;

        for (int i = 0; i < 5; ++i)
        {
            out << "juce_add_binary_data(" << kitTargets[i] << "\n"
                << "    HEADER_NAME " << kitTargets[i] << ".h\n"
                << "    NAMESPACE   " << kitTargets[i] << "\n"
                << "    SOURCES\n" << cmakeList (facts.kitData[i], 8) << ")\n\n";
            dataTargets.add (kitTargets[i]);
        }

        for (const auto& [name, files] : facts.assets)
        {
            out << "juce_add_binary_data(" << name << "\n"
                << "    HEADER_NAME " << name << ".h\n"
                << "    NAMESPACE   " << name << "\n"
                << "    SOURCES\n" << cmakeList (files, 8) << ")\n\n";
            dataTargets.add (name);
        }

        out << "target_link_libraries(" << target << "\n"
            << "    PRIVATE\n";

        for (const auto& name : dataTargets)
            out << "        " << name << "\n";

        out << "        juce::juce_audio_utils\n"
            << "        juce::juce_audio_processors\n"
            << "        juce::juce_gui_extra\n"
            << "    PUBLIC\n"
            << "        juce::juce_recommended_config_flags\n"
            << "        juce::juce_recommended_lto_flags\n"
            << "        juce::juce_recommended_warning_flags)\n\n"
            << "if(WIN32)\n"
            << "    target_link_libraries(" << target << " PRIVATE advapi32 ole32)   # the computer's ID (protection)\n"
            << "endif()\n";

        // CLAP: JUCE 9 has none of its own; clap-juce-extensions (MIT) adds it.
        juce::StringArray features;

        if (kind == 0)        features.addArray ({ "instrument", "synthesizer", "stereo" });
        else if (kind == 1)   features.addArray ({ "audio-effect", "stereo" });
        else                  features.addArray ({ "audio-effect", "note-effect", "stereo" });

        out << "\n# ------------------------------------------------------------------------------\n"
            << "# CLAP (JUCE 9 has none of its own): clap-juce-extensions (MIT, github.com/free-audio/clap-juce-extensions),\n"
            << "# yours (CLAP_JUCE_EXTENSIONS_DIR) or downloaded. -DPLUGIN_CLAP=OFF: no CLAP.\n"
            << "# ------------------------------------------------------------------------------\n"
            << "option(PLUGIN_CLAP \"Build a CLAP too (with clap-juce-extensions)\" ON)\n"
            << "set(CLAP_JUCE_EXTENSIONS_DIR \"\" CACHE PATH \"clap-juce-extensions' folder (empty: downloaded)\")\n\n"
            << "if(PLUGIN_CLAP)\n"
            << "    if(CLAP_JUCE_EXTENSIONS_DIR)\n"
            << "        add_subdirectory(\"${CLAP_JUCE_EXTENSIONS_DIR}\" clap-juce-extensions EXCLUDE_FROM_ALL)\n"
            << "    else()\n"
            << "        include(FetchContent)\n"
            << "        FetchContent_Declare(clap_juce_extensions\n"
            << "            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git\n"
            << "            GIT_TAG 6bde108329ff9fc24404b79b8b397261a75cf6a8\n"
            << "            GIT_SUBMODULES_RECURSE TRUE)\n"
            << "        FetchContent_GetProperties(clap_juce_extensions)\n\n"
            << "        if(NOT clap_juce_extensions_POPULATED)\n"
            << "            FetchContent_Populate(clap_juce_extensions)\n"
            << "            add_subdirectory(\"${clap_juce_extensions_SOURCE_DIR}\" \"${clap_juce_extensions_BINARY_DIR}\" EXCLUDE_FROM_ALL)\n"
            << "        endif()\n"
            << "    endif()\n\n"
            << "    clap_juce_extensions_plugin(TARGET " << target << "\n"
            << "        CLAP_ID " << cmakeQuoted (bundleId) << "\n"
            << "        CLAP_FEATURES " << features.joinIntoString (" ") << ")\n"
            << "endif()\n";

        out << "\n# The modules and elements, in every format.\n"
            << "foreach(_format VST3 Standalone AU CLAP)\n"
            << "    if(TARGET " << target << "_${_format})\n"
            << "        target_sources(" << target << "_${_format} PRIVATE $<TARGET_OBJECTS:" << target << "_Registered>)\n"
            << "    endif()\n"
            << "endforeach()\n";

        if (facts.isProtected)
        {
            out << "\n# A protected plugin finds its license.soni beside it (or in its bundle's Resources): put there\n"
                << "# as each format is built (in the build folder).\n"
                << "foreach(_format VST3 Standalone AU CLAP)\n"
                << "    if(TARGET " << target << "_${_format})\n"
                << "        add_custom_command(TARGET " << target << "_${_format} POST_BUILD\n"
                << "            COMMAND ${CMAKE_COMMAND} -E copy \"${CMAKE_CURRENT_SOURCE_DIR}/assets/license.soni\" \"$<TARGET_FILE_DIR:" << target << "_${_format}>/license.soni\"\n"
                << "            VERBATIM)\n"
                << "    endif()\n"
                << "endforeach()\n";
        }

        return out.toString();
    }
}

//==============================================================================
juce::Result SourceExport::write (const Request& request)
{
    const auto root = request.projectFolder.getChildFile (folderName);

    if (! root.createDirectory())
        return juce::Result::fail ("Couldn't make " + root.getFullPathName());

    Writer writer (root);
    bool ok = true;
    CMakeFacts facts;
    facts.info = request.info;

    // The kit: the plugin's own files, and Sonify's window code (SonifySourceKit).
    forEachResource (SonifySourceKit::namedResourceList, SonifySourceKit::namedResourceListSize, SonifySourceKit::getNamedResource,
                     SonifySourceKit::getNamedResourceOriginalFilename, [&] (const juce::String& name, const char* data, int size)
    {
        juce::String path;

        if (name.startsWith ("kit_Plugin"))         path = "plugin/" + name.substring (4);
        else if (name.startsWith ("kit_"))          path = "gui/" + name.substring (4);
        else if (name == "inter_license.txt")       path = "gui/data/inter/" + name;
        else                                        path = "gui/" + name;

        ok = (isSourceFile (name) ? writer.putSource (path, data, size) : writer.put (path, data, (size_t) size)) && ok;

        if (name.endsWith (".cpp"))
            (path.startsWith ("plugin/") ? facts.pluginSources : facts.guiSources).add (path);
    });

    // The window code's data.
    forEachResource (StellaLooks::namedResourceList, StellaLooks::namedResourceListSize, StellaLooks::getNamedResource,
                     StellaLooks::getNamedResourceOriginalFilename, [&] (const juce::String& name, const char* data, int size)
    {
        ok = writer.put ("gui/data/looks/" + name, data, (size_t) size) && ok;
        facts.kitData[0].add ("gui/data/looks/" + name);
    });

    forEachResource (StellaPrimitives::namedResourceList, StellaPrimitives::namedResourceListSize, StellaPrimitives::getNamedResource,
                     StellaPrimitives::getNamedResourceOriginalFilename, [&] (const juce::String& name, const char* data, int size)
    {
        ok = writer.put ("gui/data/primitives/" + name, data, (size_t) size) && ok;
        facts.kitData[1].add ("gui/data/primitives/" + name);
    });

    forEachResource (SonifyFonts::namedResourceList, SonifyFonts::namedResourceListSize, SonifyFonts::getNamedResource,
                     SonifyFonts::getNamedResourceOriginalFilename, [&] (const juce::String& name, const char* data, int size)
    {
        ok = writer.put ("gui/data/fonts/" + name, data, (size_t) size) && ok;
        facts.kitData[2].add ("gui/data/fonts/" + name);
    });

    forEachResource (SonifyIcons::namedResourceList, SonifyIcons::namedResourceListSize, SonifyIcons::getNamedResource,
                     SonifyIcons::getNamedResourceOriginalFilename, [&] (const juce::String& name, const char* data, int size)
    {
        ok = writer.put ("gui/data/icons/" + name, data, (size_t) size) && ok;
        facts.kitData[3].add ("gui/data/icons/" + name);
    });

    ok = writer.put ("gui/data/inter/inter_regular.ttf", StellaRuntimeData::inter_regular_ttf, (size_t) StellaRuntimeData::inter_regular_ttfSize) && ok;
    ok = writer.put ("gui/data/inter/inter_bold.ttf", StellaRuntimeData::inter_bold_ttf, (size_t) StellaRuntimeData::inter_bold_ttfSize) && ok;
    facts.kitData[4].addArray ({ "gui/data/inter/inter_regular.ttf", "gui/data/inter/inter_bold.ttf", "gui/data/inter/inter_license.txt" });

    ok = writer.putSource ("gui/stella_keys.h", StellaRuntimeData::stella_keys_h, StellaRuntimeData::stella_keys_hSize) && ok;

    // The Stella runtime and the programmed elements' runtime, as the studio builds with them.
    {
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sonify")
                              .getChildFile ("source-runtime-" + juce::String::toHexString ((juce::int64) juce::Random::getSystemRandom().nextInt64()));
        temp.createDirectory();

        if (WasmCompiler::writeRuntime (temp) && WasmCompiler::writeElementRuntime (temp))
        {
            for (const auto& file : temp.findChildFiles (juce::File::findFiles, false))
            {
                juce::MemoryBlock data;
                file.loadFileAsData (data);
                const auto path = "stella/" + file.getFileName();
                ok = (isSourceFile (file.getFileName()) ? writer.putSource (path, static_cast<const char*> (data.getData()), (int) data.getSize())
                                                       : writer.put (path, data)) && ok;

                if (file.getFileName().startsWith ("stella_builtin_") && file.getFileName().endsWith (".cpp"))
                    facts.registeredSources.add (path);   // registers itself (STELLA_BUILTIN_ELEMENT)
                else if (file.getFileName().endsWith (".cpp"))
                    facts.stellaSources.add (path);
            }
        }
        else
        {
            ok = false;
        }

        temp.deleteRecursively();
    }

    // The graph, as C++.
    {
        juce::String graph;

        if (const auto made = WasmCompiler::generateGraphSource (request.projectFolder.getChildFile (WasmCompiler::graphFileName), graph); made.failed())
            return juce::Result::fail ("The plugin's graph: " + made.getErrorMessage());

        ok = writer.putText ("stella/stella_graph.cpp", graph) && ok;
        facts.stellaSources.add ("stella/stella_graph.cpp");
    }

    // Protected: its product and demo, and its license.soni.
    const auto protection = Licences::loadProtection (request.projectFolder);
    facts.isProtected = protection.isValid();

    if (facts.isProtected)
    {
        ok = writer.putText ("stella/stella_product.h", productSource (protection)) && ok;
        ok = writer.put ("assets/license.soni", Licences::soniData (Licences::soniFor (protection, request.info.name, request.info.vendor))) && ok;
    }

    facts.stellaSources.sort (true);
    facts.registeredSources.sort (true);

    // The modules and the programmed elements, as the project has them.
    for (const auto* sourceFolder : { "modules", "elements" })
    {
        const auto from = request.projectFolder.getChildFile (sourceFolder);

        for (const auto& file : from.findChildFiles (juce::File::findFiles, true))
        {
            if (file.isHidden() || file.getFileName().startsWithChar ('.'))
                continue;

            const auto path = juce::String (sourceFolder) + "/" + file.getRelativePathFrom (from).replaceCharacter ('\\', '/');
            ok = writer.putFile (path, file) && ok;

            if (file.hasFileExtension ("cpp") && file.getParentDirectory() == from)
                (juce::String (sourceFolder) == "modules" ? facts.moduleSources : facts.elementSources).add (path);
        }
    }

    facts.moduleSources.sort (true);
    facts.elementSources.sort (true);

    // What's this plugin's own, as C++.
    PresetBank bank;
    bank.load (request.projectFolder);
    const auto library = PluginLibrary::load (request.projectFolder);

    ok = writer.putText ("plugin/PluginInfo.h", infoSource (request.info, library, facts.isProtected)) && ok;
    ok = writer.putText ("plugin/PluginWindow.cpp", windowSource (request.layout, request.info.name)) && ok;
    ok = writer.putText ("plugin/PluginPresets.cpp", presetsSource (bank.presets, request.info.name)) && ok;
    facts.pluginSources.addArray ({ "plugin/PluginWindow.cpp", "plugin/PluginPresets.cpp", "plugin/PluginAssets.cpp" });

    // What it carries: its pictures, looks, and what its looks draw from.
    const auto gui = request.projectFolder.getChildFile ("gui");

    for (const auto& file : gui.getChildFile (GuiLayout::imagesFolder).findChildFiles (juce::File::findFiles, false))
        if (! file.isHidden() && ! file.getFileName().startsWithChar ('.'))
            ok = writer.putFile ("assets/images/" + file.getFileName(), file) && ok;

    for (const auto& picture : request.pictures)
        if (! gui.getChildFile (GuiLayout::imagesFolder).getChildFile (picture.getFileName()).existsAsFile())
            ok = writer.putFile ("assets/images/" + picture.getFileName(), picture) && ok;

    for (const auto& [name, data] : request.ownLooks)
        ok = writer.put ("assets/looks/" + name, data) && ok;

    for (const auto& [name, data] : request.myLooks)
        ok = writer.put ("assets/mylooks/" + name, data) && ok;

    for (const auto& shape : request.shapes)
        ok = writer.putFile ("assets/shapes/" + shape.getFileName(), shape) && ok;

    for (const auto& texture : request.textures)
        ok = writer.putFile ("assets/textures/" + texture.getFileName(), texture) && ok;

    juce::StringArray presentAssets;

    for (const auto& folder : assetFolders)
    {
        if (const auto files = writer.filesIn (juce::String ("assets/") + folder.source); ! files.isEmpty())
        {
            presentAssets.add (folder.source);
            facts.assets.push_back ({ folder.target, files });
        }
    }

    ok = writer.putText ("plugin/PluginAssets.cpp", assetsSource (presentAssets)) && ok;

    // Pictures of the window, page by page.
    for (size_t page = 0; page < request.pages.size(); ++page)
    {
        if (! request.pages[page].isValid())
            continue;

        const auto name = request.layout.pages.isEmpty() ? juce::String ("window")
                                                         : juce::String ((int) page + 1) + " " + juce::File::createLegalFileName (request.layout.pages[(int) page]);
        juce::MemoryOutputStream png;
        juce::PNGImageFormat().writeImageToStream (request.pages[page], png);
        ok = writer.put ("preview/" + name + ".png", png.getData(), png.getDataSize()) && ok;
    }

    for (auto* list : { &facts.pluginSources, &facts.guiSources })
        list->sort (true);

    ok = writer.putText ("CMakeLists.txt", cmakeSource (facts)) && ok;

    writer.removeStale ({ "plugin", "gui", "stella", "modules", "elements", "assets", "preview" }, { "CMakeLists.txt" });

    return ok ? juce::Result::ok() : juce::Result::fail ("Some files couldn't be written into " + root.getFullPathName());
}
