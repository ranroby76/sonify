// D:\workspace\Fanan_web_hub\src\PluginExporter.cpp

#include "PluginExporter.h"
#include "FontLibrary.h"
#include "Licences.h"
#include "WasmCompiler.h"

#include "StellaRuntimeData.h"

#include <thread>

namespace
{
    //==========================================================================
    // Where things are
    juce::File besideProgram (const juce::String& name)
    {
        const auto program = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

       #if JUCE_MAC
        // Sonify.app/Contents/MacOS/Sonify: what ships with it is in its Resources.
        return program.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile (name);
       #else
        return program.getParentDirectory().getChildFile (name);
       #endif
    }

   #if ! JUCE_WINDOWS && ! JUCE_MAC
    juce::File findInPath (const juce::String& name)
    {
        const auto path = juce::SystemStats::getEnvironmentVariable ("PATH", "/usr/local/bin:/usr/bin:/bin");

        for (const auto& folder : juce::StringArray::fromTokens (path, ":", ""))
            if (folder.isNotEmpty())
                if (const auto file = juce::File (folder).getChildFile (name); file.existsAsFile())
                    return file;

        return {};
    }
   #endif

   #if JUCE_MAC
    /** Runs a command and gives what it printed; exitCode -1 if it couldn't start. */
    juce::String run (const juce::StringArray& args, int& exitCode, int timeoutMs = 30000)
    {
        juce::ChildProcess process;
        exitCode = -1;

        if (! process.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            return {};

        const auto output = process.readAllProcessOutput();
        process.waitForProcessToFinish (timeoutMs);
        exitCode = (int) process.getExitCode();
        return output;
    }
   #endif

    /** The compiler, as it's run: its program and what goes before the rest. */
    juce::StringArray compilerCommand()
    {
       #if JUCE_WINDOWS
        for (const auto& folder : { besideProgram ("Plugin Compiler"),
                                   #ifdef FANAN_PLUGIN_COMPILER_DIR
                                    juce::File (FANAN_PLUGIN_COMPILER_DIR),
                                   #endif
                                    juce::File() })
        {
            const auto compiler = folder.getChildFile ("bin").getChildFile ("x86_64-w64-mingw32-clang++.exe");

            if (folder != juce::File() && compiler.existsAsFile())
                return { compiler.getFullPathName() };
        }

        return {};
       #elif JUCE_MAC
        // Apple's, from its Command Line Tools (or Xcode), found by xcrun with its SDK.
        int code = -1;
        const auto folder = run ({ "/usr/bin/xcode-select", "-p" }, code).trim();

        if (code != 0 || folder.isEmpty() || ! juce::File (folder).isDirectory())
            return {};

        run ({ "/usr/bin/xcrun", "--find", "clang++" }, code);
        return code == 0 ? juce::StringArray { "/usr/bin/xcrun", "clang++" } : juce::StringArray();
       #else
        for (const auto* name : { "c++", "g++", "clang++" })
            if (const auto compiler = findInPath (name); compiler.existsAsFile())
                return { compiler.getFullPathName() };

        return {};
       #endif
    }

   #if ! JUCE_WINDOWS && ! JUCE_MAC
    juce::String linuxArchitecture()
    {
       #if defined (__aarch64__)
        return "aarch64-linux";
       #else
        return "x86_64-linux";
       #endif
    }
   #endif

    /** The user's own folder for plugins of a format, where DAWs look (no admin rights needed). */
    juce::File installFolder (PluginExporter::Format format)
    {
        using Format = PluginExporter::Format;

       #if JUCE_WINDOWS
        return juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("Programs").getChildFile ("Common")
                   .getChildFile (format == Format::vst3 ? "VST3" : "CLAP");
       #elif JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library").getChildFile ("Audio").getChildFile ("Plug-Ins")
                   .getChildFile (format == Format::vst3 ? "VST3" : (format == Format::clap ? "CLAP" : "Components"));
       #else
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (format == Format::vst3 ? ".vst3" : ".clap");
       #endif
    }

    //==========================================================================
    // C++ text
    /** A C++ string literal for plain text (ids, which are ASCII). */
    juce::String literal (const juce::String& text)
    {
        juce::String result ("\"");

        for (auto c : text)
        {
            if (c == '"' || c == '\\')
                result << '\\' << juce::String::charToString (c);
            else if (c >= 32 && c < 127)
                result << juce::String::charToString (c);
        }

        return result + "\"";
    }

    juce::String literalOrNull (const juce::String& text)
    {
        return text.isNotEmpty() ? literal (text) : juce::String ("nullptr");
    }

    /** Any text as a C++ string literal, all of it: bytes outside plain ASCII as octal escapes of
        its UTF-8, so a label in any language reaches the plugin as it is. */
    juce::String textLiteral (const juce::String& text)
    {
        juce::String result ("\"");
        const auto* utf8 = text.toRawUTF8();

        for (size_t i = 0; utf8[i] != 0; ++i)
        {
            const auto c = (unsigned char) utf8[i];

            if (c == '"' || c == '\\')
            {
                result << "\\" << juce::String::charToString ((juce::juce_wchar) c);
            }
            else if (c >= 32 && c < 127 && c != '?')   // '?' too: no trigraphs
            {
                result << juce::String::charToString ((juce::juce_wchar) c);
            }
            else
            {
                const char octal[] { '\\', (char) ('0' + (c >> 6)), (char) ('0' + ((c >> 3) & 7)), (char) ('0' + (c & 7)), 0 };
                result << octal;
            }
        }

        return result + "\"";
    }

    /** A list of texts as a C++ array of string literals. */
    void appendTextArray (juce::String& out, const juce::String& name, const juce::StringArray& items)
    {
        out << "    static const char* const " << name << "[] { ";

        for (const auto& item : items)
            out << textLiteral (item) << ", ";

        out << (items.isEmpty() ? "nullptr };\n" : "};\n");
    }

    /** Bytes as a C++ string literal, octal escapes 64 bytes to a line (quick to compile, even
        for big pictures). */
    void appendBytes (juce::MemoryOutputStream& out, const void* data, size_t size)
    {
        const auto* bytes = static_cast<const juce::uint8*> (data);
        char line[64 * 4 + 16];

        for (size_t i = 0; i < size; i += 64)
        {
            auto* c = line;
            *c++ = ' '; *c++ = ' '; *c++ = ' '; *c++ = ' '; *c++ = ' '; *c++ = ' '; *c++ = ' '; *c++ = ' ';
            *c++ = '"';

            for (size_t j = i; j < std::min (size, i + 64); ++j)
            {
                *c++ = '\\';
                *c++ = (char) ('0' + (bytes[j] >> 6));
                *c++ = (char) ('0' + ((bytes[j] >> 3) & 7));
                *c++ = (char) ('0' + (bytes[j] & 7));
            }

            *c++ = '"';
            *c++ = '\n';
            out.write (line, (size_t) (c - line));
        }

        if (size == 0)
            out << "        \"\"\n";
    }

    void appendBytes (juce::MemoryOutputStream& out, const juce::MemoryBlock& data)
    {
        appendBytes (out, data.getData(), data.getSize());
    }

    juce::MemoryBlock png (const juce::Image& image)
    {
        juce::MemoryOutputStream out;
        juce::PNGImageFormat().writeImageToStream (image, out);
        return out.getMemoryBlock();
    }

    /** A font's name as the elements ask for it (lower case, no spaces), as stella_gui.cpp has it. */
    juce::String fontKey (const juce::String& name)
    {
        return name.toLowerCase().removeCharacters (" -_");
    }

    /** The baked window as C++. withRegistration: a protected plugin's, with its registration
        badge (the rest leave it out, its frames too). */
    juce::String guiSource (const PluginExporter::Gui& gui, bool withRegistration)
    {
        juce::MemoryOutputStream out;
        out << "// Generated by Sonify's Plugins Creator Studio for this export: the plugin's window, baked.\n\n"
            << "#include \"stella_gui.h\"\n\nnamespace stella::gui\n{\n";

        out << "    extern const int width = " << gui.width << ", height = " << gui.height << ";\n\n";

        for (size_t i = 0; i < gui.pages.size(); ++i)
        {
            out << "    static const unsigned char page" << (int) i << "[] =\n";
            appendBytes (out, gui.pages[i]);
            out << "    ;\n\n";
        }

        out << "    extern const Png pages[]\n    {\n";

        for (size_t i = 0; i < gui.pages.size(); ++i)
            out << "        { page" << (int) i << ", " << (int) gui.pages[i].getSize() << " },\n";

        if (gui.pages.empty())
            out << "        { nullptr, 0 },\n";

        out << "    };\n\n    extern const int numPages = " << (int) gui.pages.size() << ";\n\n";

        auto stripIsIn = [&gui, withRegistration] (size_t i)    { return withRegistration || ! gui.strips[i].registration; };

        for (size_t i = 0; i < gui.strips.size(); ++i)
        {
            if (! stripIsIn (i))
                continue;

            out << "    static const unsigned char strip" << (int) i << "[] =\n";
            appendBytes (out, gui.strips[i].png);
            out << "    ;\n\n";
        }

        out << "    extern const Strip strips[]\n    {\n";

        // A strip left out keeps its place (the widgets name strips by number), with no frames.
        for (size_t i = 0; i < gui.strips.size(); ++i)
            out << "        { " << (stripIsIn (i) ? "strip" + juce::String ((int) i) : juce::String ("nullptr")) << ", "
                << (stripIsIn (i) ? (int) gui.strips[i].png.getSize() : 0) << ", " << gui.strips[i].frameWidth << ", "
                << gui.strips[i].frameHeight << ", " << gui.strips[i].frames << " },\n";

        if (gui.strips.empty())
            out << "        { nullptr, 0, 0, 0, 0 },\n";

        out << "    };\n\n    extern const int numStrips = " << (int) gui.strips.size() << ";\n\n";

        // The registration badge last: drawn over everything, and the others' numbers kept.
        const auto numWidgets = gui.numWidgets + (withRegistration ? gui.numRegistration : 0);
        out << "    extern const Widget widgets[]\n    {\n" << gui.widgetRows << (withRegistration ? gui.registrationRows : juce::String());

        if (numWidgets == 0)
            out << "        { Kind::knob, 0, 0, 0, 0, -1, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0.0f, 0, 0, 0.0f, -1, 0, 0, 0, 0, 0.0f },\n";

        out << "    };\n\n    extern const int numWidgets = " << numWidgets << ";\n\n";

        out << gui.presetArrays << "    extern const Preset presets[]\n    {\n" << gui.presetRows;

        if (gui.numPresets == 0)
            out << "        { nullptr, nullptr, 0 },\n";

        out << "    };\n\n    extern const int numPresets = " << gui.numPresets << ";\n\n";

        out << gui.customArrays << "    extern const CustomElement customElements[]\n    {\n" << gui.customRows;

        if (gui.numCustom == 0)
            out << "        { \"\", \"\", nullptr, 0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0 },\n";

        out << "    };\n\n    extern const int numCustomElements = " << gui.numCustom << ";\n\n";

        out << "    extern const RoleSource roleSources[]\n    {\n" << gui.roleSourceRows;

        if (gui.numRoleSources == 0)
            out << "        { -1, nullptr, nullptr, 0 },\n";

        out << "    };\n\n    extern const int numRoleSources = " << gui.numRoleSources << ";\n\n";

        for (size_t i = 0; i < gui.pictures.size(); ++i)
        {
            out << "    static const unsigned char picture" << (int) i << "[] =\n";
            appendBytes (out, gui.pictures[i].data);
            out << "    ;\n\n";
        }

        out << "    extern const Picture pictures[]\n    {\n";

        for (size_t i = 0; i < gui.pictures.size(); ++i)
            out << "        { " << textLiteral (gui.pictures[i].name) << ", picture" << (int) i << ", " << (int) gui.pictures[i].data.getSize() << " },\n";

        if (gui.pictures.empty())
            out << "        { nullptr, nullptr, 0 },\n";

        out << "    };\n\n    extern const int numPictures = " << (int) gui.pictures.size() << ";\n\n";

        for (size_t i = 0; i < gui.fonts.size(); ++i)
        {
            out << "    static const unsigned char font" << (int) i << "[] =\n";
            appendBytes (out, gui.fonts[i].data);
            out << "    ;\n\n";
        }

        out << "    extern const FontFile fonts[]\n    {\n";

        for (size_t i = 0; i < gui.fonts.size(); ++i)
            out << "        { " << literal (gui.fonts[i].name) << ", " << (gui.fonts[i].bold ? 1 : 0) << ", font" << (int) i << ", "
                << (int) gui.fonts[i].data.getSize() << " },\n";

        if (gui.fonts.empty())
            out << "        { nullptr, 0, nullptr, 0 },\n";

        out << "    };\n\n    extern const int numFonts = " << (int) gui.fonts.size() << ";\n}\n";
        return out.toString();
    }

    //==========================================================================
    // macOS bundles
    std::uint32_t fnv (const juce::String& text)
    {
        std::uint32_t hash = 2166136261u;

        for (const auto* c = text.toRawUTF8(); *c != 0; ++c)
        {
            hash ^= (std::uint8_t) *c;
            hash *= 16777619u;
        }

        return hash;
    }

    /** A four-character code from a hash, in letters and digits (first: first, when given). */
    juce::String fourCharCode (std::uint32_t hash, juce::juce_wchar first = 0)
    {
        static const char* const characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        juce::String code;

        if (first != 0)
            code << juce::String::charToString (first);

        while (code.length() < 4)
        {
            code << juce::String::charToString ((juce::juce_wchar) characters[hash % 62]);
            hash = hash / 62 + 0x9e3779b9u * (std::uint32_t) code.length();
        }

        return code;
    }

    juce::String hexOf (const juce::String& code)
    {
        std::uint32_t value = 0;

        for (int i = 0; i < 4; ++i)
            value = (value << 8) | (std::uint32_t) (code[i] & 0xff);

        return "0x" + juce::String::toHexString ((juce::int64) value).paddedLeft ('0', 8) + "u";
    }

   #if JUCE_MAC
    int versionNumber (const juce::String& version)
    {
        const auto parts = juce::StringArray::fromTokens (version, ".", "");
        const auto part = [&] (int i) { return juce::jlimit (0, 255, parts[i].getIntValue()); };
        return (part (0) << 16) | (part (1) << 8) | part (2);
    }

    juce::String xml (const juce::String& text)
    {
        return text.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\"", "&quot;");
    }

    juce::String bundleId (const ProjectInfo& info, const juce::String& ending)
    {
        juce::String id;

        for (auto c : (info.pluginId.isNotEmpty() ? info.pluginId : "com.sonify." + info.name))
            id << (juce::CharacterFunctions::isLetterOrDigit (c) && c < 128 ? juce::String::charToString (c) : (c == '.' ? juce::String (".") : juce::String ("-")));

        return id + "." + ending;
    }

    juce::String infoPlist (const ProjectInfo& info, const juce::String& executable, const juce::String& ending,
                            const juce::String& auType, const juce::String& auSubtype, const juce::String& auManufacturer)
    {
        juce::String plist;
        plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
              << "<plist version=\"1.0\">\n<dict>\n"
              << "    <key>CFBundleDevelopmentRegion</key><string>English</string>\n"
              << "    <key>CFBundleExecutable</key><string>" << xml (executable) << "</string>\n"
              << "    <key>CFBundleIdentifier</key><string>" << xml (bundleId (info, ending)) << "</string>\n"
              << "    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>\n"
              << "    <key>CFBundleName</key><string>" << xml (info.name) << "</string>\n"
              << "    <key>CFBundlePackageType</key><string>BNDL</string>\n"
              << "    <key>CFBundleShortVersionString</key><string>" << xml (info.version) << "</string>\n"
              << "    <key>CFBundleVersion</key><string>" << xml (info.version) << "</string>\n"
              << "    <key>CFBundleSignature</key><string>???" "?</string>\n"   // (split: no trigraph)
              << "    <key>NSHighResolutionCapable</key><true/>\n";

        if (auType.isNotEmpty())
        {
            plist << "    <key>AudioComponents</key>\n    <array>\n        <dict>\n"
                  << "            <key>type</key><string>" << auType << "</string>\n"
                  << "            <key>subtype</key><string>" << xml (auSubtype) << "</string>\n"
                  << "            <key>manufacturer</key><string>" << xml (auManufacturer) << "</string>\n"
                  << "            <key>name</key><string>" << xml (info.vendor + ": " + info.name) << "</string>\n"
                  << "            <key>description</key><string>" << xml (info.name) << "</string>\n"
                  << "            <key>version</key><integer>" << versionNumber (info.version) << "</integer>\n"
                  << "            <key>factoryFunction</key><string>StellaAudioUnitFactory</string>\n"
                  << "            <key>sandboxSafe</key><true/>\n"
                  << "            <key>tags</key><array><string>" << (auType == "aumu" ? "Synthesizer" : "Effects") << "</string></array>\n"
                  << "        </dict>\n    </array>\n";
        }

        plist << "</dict>\n</plist>\n";
        return plist;
    }
   #endif
}

//==============================================================================
juce::Array<PluginExporter::Format> PluginExporter::formatsHere()
{
   #if JUCE_MAC
    return { Format::vst3, Format::au, Format::clap };
   #else
    return { Format::vst3, Format::clap };
   #endif
}

juce::String PluginExporter::formatName (Format format)
{
    switch (format)
    {
        case Format::vst3:  return "VST3";
        case Format::clap:  return "CLAP";
        case Format::au:    return "Audio Unit";
    }

    return {};
}

juce::String PluginExporter::fileNameFor (Format format, const juce::String& pluginName)
{
    const auto base = juce::File::createLegalFileName (pluginName);

    switch (format)
    {
        case Format::vst3:  return base + ".vst3";
        case Format::clap:  return base + ".clap";
        case Format::au:    return base + ".component";
    }

    return base;
}

juce::File PluginExporter::defaultExportFolder (const juce::String& pluginName)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Sonify")
               .getChildFile ("Exports")
               .getChildFile (juce::File::createLegalFileName (pluginName));
}

juce::String PluginExporter::installFoldersText()
{
    juce::StringArray folders;

    for (const auto format : formatsHere())
        folders.add (installFolder (format).getFullPathName());

    return folders.joinIntoString ("\n");
}

juce::File PluginExporter::findClapHeaders()
{
    for (const auto& candidate : { besideProgram ("Plugin SDKs").getChildFile ("clap").getChildFile ("include"),
                                  #ifdef FANAN_PLUGIN_CLAP_DIR
                                   juce::File (FANAN_PLUGIN_CLAP_DIR),
                                  #endif
                                   juce::File() })
        if (candidate != juce::File() && candidate.getChildFile ("clap").getChildFile ("clap.h").existsAsFile())
            return candidate;

    return {};
}

juce::File PluginExporter::findVst3Headers()
{
    for (const auto& candidate : { besideProgram ("Plugin SDKs").getChildFile ("vst3"),
                                  #ifdef FANAN_PLUGIN_VST3_DIR
                                   juce::File (FANAN_PLUGIN_VST3_DIR),
                                  #endif
                                   juce::File() })
        if (candidate != juce::File() && candidate.getChildFile ("pluginterfaces").getChildFile ("base").getChildFile ("funknown.h").existsAsFile())
            return candidate;

    return {};
}

juce::String PluginExporter::compilerProblem()
{
    if (compilerCommand().isEmpty())
    {
       #if JUCE_WINDOWS
        return "The plugin compiler that comes with Sonify is missing (the Plugin Compiler folder beside Sonify.exe). Reinstall Sonify.";
       #elif JUCE_MAC
        // Apple's installer for its compiler: the user says yes, and exports again once it's in.
        juce::ChildProcess installer;
        installer.start (juce::StringArray { "/usr/bin/xcode-select", "--install" }, 0);
        return "Exporting on a Mac needs Apple's Command Line Tools (its free compiler). macOS's installer for them has opened: "
               "install them, then export again.";
       #else
        return "Exporting on Linux needs a C++ compiler: install one (build-essential on Ubuntu and Debian, gcc-c++ on Fedora), "
               "then export again.";
       #endif
    }

    if (findClapHeaders() == juce::File() || findVst3Headers() == juce::File())
        return "The plugin formats' headers that come with Sonify are missing (the Plugin SDKs folder). Reinstall Sonify.";

    return {};
}

//==============================================================================
PluginExporter::Gui PluginExporter::prepareGui (const GuiLayout& layout, const PluginCanvas::ExportBake& bake,
                                                const std::map<juce::String, juce::String>& unitsById, const std::map<juce::String, juce::String>& namesById,
                                                const juce::StringArray& displayIds, const std::vector<Preset>& presets, const juce::File& guiFolder)
{
    Gui gui;

    // The presets go in even without a window (the window's preset managers and an Audio Unit's
    // factory presets use them).
    for (size_t i = 0; i < presets.size(); ++i)
    {
        gui.presetArrays << "    static const PresetValue preset" << (int) i << "[] =\n    {\n";

        for (const auto& [id, value] : presets[i].values)
            gui.presetArrays << "        { " << literal (id) << ", " << juce::String (value, 6) << "f },\n";

        if (presets[i].values.empty())
            gui.presetArrays << "        { nullptr, 0.0f },\n";

        gui.presetArrays << "    };\n\n";
        gui.presetRows << "        { " << textLiteral (presets[i].name) << ", preset" << (int) i << ", " << (int) presets[i].values.size() << " },\n";
        ++gui.numPresets;
    }

    if (layout.widgets.empty() || bake.pages.empty() || bake.width <= 0 || bake.height <= 0)
        return gui;

    gui.width = bake.width;
    gui.height = bake.height;

    for (const auto& page : bake.pages)
        gui.pages.push_back (png (page));

    for (const auto& strip : bake.strips)
        gui.strips.push_back ({ png (strip.image), strip.frameWidth, strip.frameHeight, strip.frames });

    auto role = [] (const GuiWidget& w, const char* name)
    {
        const auto found = w.roles.find (name);
        return found != w.roles.end() ? found->second : juce::String();
    };

    juce::StringArray fontNames;

    for (const auto& item : bake.items)
    {
        if (! juce::isPositiveAndBelow (item.widget, (int) layout.widgets.size()))
            continue;

        const auto& w = layout.widgets[(size_t) item.widget];
        juce::String p1, p2, p3, p4, source;

        switch (w.type)
        {
            case GuiWidget::Type::knob:
            case GuiWidget::Type::slider:
            case GuiWidget::Type::toggle:
            case GuiWidget::Type::selector:
            case GuiWidget::Type::label:     p1 = w.param; break;
            case GuiWidget::Type::xy:        p1 = role (w, "x"); p2 = role (w, "y"); break;
            case GuiWidget::Type::envelope:  p1 = role (w, "attack"); p2 = role (w, "decay"); p3 = role (w, "sustain"); p4 = role (w, "release"); break;
            case GuiWidget::Type::filter:    p1 = role (w, "cutoff"); p2 = role (w, "resonance"); source = w.source; break;   // the live cutoff it follows
            case GuiWidget::Type::meter:
            case GuiWidget::Type::lamp:
            case GuiWidget::Type::scope:     source = w.source; break;
            case GuiWidget::Type::custom:    p1 = w.param; source = w.source; break;
            case GuiWidget::Type::group:
            case GuiWidget::Type::shape:
            case GuiWidget::Type::image:
            case GuiWidget::Type::preset:
            case GuiWidget::Type::keyboard:
            case GuiWidget::Type::registration:
            case GuiWidget::Type::tabs:      break;
        }

        // A layer in front of something that moves is a picture, whatever it is.
        if (item.kind == "picture")
            p1 = p2 = p3 = p4 = source = {};

        // A programmed element without a colour picks its own.
        const auto colour = w.type == GuiWidget::Type::custom ? (w.colour.isTransparent() ? 0u : w.colour.getARGB())
                                                               : (w.colour.isTransparent() ? juce::Colour (0xffe5484d) : w.colour).getARGB();
        auto count = item.count;

        // A programmed element: what its widget tells it, in arrays of its own.
        if (item.kind == "custom")
        {
            count = gui.numCustom;
            const auto prefix = "custom" + juce::String (gui.numCustom);
            juce::StringArray roles, roleParams, names, units, keys, settings;

            auto describe = [&] (const juce::String& id)
            {
                names.add (namesById.count (id) > 0 ? namesById.at (id) : juce::String());
                units.add (unitsById.count (id) > 0 ? unitsById.at (id) : juce::String());
            };

            describe (w.param);

            for (const auto& [roleName, id] : w.roles)
            {
                roles.add (roleName);
                roleParams.add (id);
                describe (id);
            }

            for (const auto& entry : w.settings)
            {
                keys.add (entry.first);
                settings.add (w.settingText (entry.first));

                if (entry.first == "font")
                    fontNames.addIfNotAlreadyThere (w.settingText (entry.first));
            }

            if (w.font.isNotEmpty())
                fontNames.addIfNotAlreadyThere (w.font);

            // Its font is the element's own field when its settings name none: the studio answers
            // "font" with it (ElementBridge::text), so the exported window must too.
            if (w.settings.find ("font") == w.settings.end())
            {
                if (const auto font = FontLibrary::canonical (w.font); font.isNotEmpty())
                {
                    keys.add ("font");
                    settings.add (font);
                }
            }

            appendTextArray (gui.customArrays, prefix + "Options", w.options);
            appendTextArray (gui.customArrays, prefix + "Roles", roles);
            appendTextArray (gui.customArrays, prefix + "RoleParams", roleParams);
            appendTextArray (gui.customArrays, prefix + "Names", names);
            appendTextArray (gui.customArrays, prefix + "Units", units);
            appendTextArray (gui.customArrays, prefix + "Keys", keys);
            appendTextArray (gui.customArrays, prefix + "Values", settings);
            gui.customArrays << "\n";

            gui.customRows << "        { " << textLiteral (w.element) << ", " << textLiteral (w.label) << ", "
                           << prefix << "Options, " << w.options.size() << ", "
                           << prefix << "Roles, " << prefix << "RoleParams, " << roles.size() << ", "
                           << prefix << "Names, " << prefix << "Units, "
                           << prefix << "Keys, " << prefix << "Values, " << keys.size() << " },\n";

            // What else it watches, by role.
            for (const auto& [roleName, watched] : w.sources)
            {
                if (watched.isEmpty())
                    continue;

                gui.roleSourceRows << "        { " << gui.numWidgets << ", " << literal (roleName) << ", " << literal (watched) << ", "
                                   << (displayIds.contains (watched) ? 1 : 0) << " },\n";
                ++gui.numRoleSources;
            }

            gui.elementTypes.addIfNotAlreadyThere (w.element);
            ++gui.numCustom;
        }

        float cutoffScale = 0.0f;

        if (w.type == GuiWidget::Type::filter)
        {
            const auto unit = unitsById.count (p1) > 0 ? unitsById.at (p1) : juce::String();
            cutoffScale = unit.equalsIgnoreCase ("Hz") ? 1.0f : (unit.equalsIgnoreCase ("kHz") ? 1000.0f : 0.0f);
        }

        const auto vertical = w.type == GuiWidget::Type::slider && w.vertical ? 1 : 0;
        const auto& a = item.area;
        const auto& b = item.box;

        // The registration badge goes in a protected plugin only (guiSource): kept apart.
        const bool isBadge = item.kind == "registration";

        if (isBadge && juce::isPositiveAndBelow (item.strip, (int) gui.strips.size()))
            gui.strips[(size_t) item.strip].registration = true;

        (isBadge ? gui.registrationRows : gui.widgetRows) << "        { Kind::" << item.kind << ", " << a.getX() << ", " << a.getY() << ", " << a.getWidth() << ", " << a.getHeight()
                       << ", " << item.strip << ", " << literalOrNull (p1) << ", " << literalOrNull (p2) << ", " << literalOrNull (p3) << ", "
                       << literalOrNull (p4) << ", " << literalOrNull (source) << ", 0x" << juce::String::toHexString ((juce::int64) colour) << "u, "
                       << vertical << ", " << count << ", " << juce::String (w.threshold, 4) << "f, " << item.mode << ", "
                       << (displayIds.contains (source) ? 1 : 0) << ", " << juce::String (cutoffScale, 1) << "f, " << item.page << ", "
                       << b.getX() << ", " << b.getY() << ", " << b.getWidth() << ", " << b.getHeight() << ", "
                       << juce::String (item.angle, 3) << "f },\n";

        if (isBadge)
            ++gui.numRegistration;
        else
            ++gui.numWidgets;
    }

    // Programmed elements may ask for the plugin's pictures and fonts by name.
    if (gui.numCustom > 0)
    {
        for (const auto& file : guiFolder.getChildFile ("images").findChildFiles (juce::File::findFiles, false, "*.png;*.jpg;*.jpeg;*.gif;*.bmp"))
        {
            const auto image = juce::ImageFileFormat::loadFrom (file);

            if (image.isValid() && image.getWidth() <= 4096 && image.getHeight() <= 4096)
                gui.pictures.push_back ({ file.getFileName(), false, png (image) });
        }

        for (const auto& name : fontNames)
        {
            const auto* family = FontLibrary::find (name);

            if (family == nullptr)
                continue;

            for (const bool bold : { false, true })
            {
                const void* data = nullptr;
                int size = 0;

                if ((bold && (family->bold == nullptr || family->boldSize <= 0)) || ! FontLibrary::file (family->name, bold, data, size) || size <= 0)
                    continue;

                for (const auto& key : juce::StringArray { fontKey (name), fontKey (family->name) })
                {
                    bool already = false;

                    for (const auto& f : gui.fonts)
                        already = already || (f.name == key && f.bold == bold);

                    if (! already)
                        gui.fonts.push_back ({ key, bold, juce::MemoryBlock (data, (size_t) size) });
                }
            }
        }
    }

    return gui;
}

//==============================================================================
PluginExporter::Result PluginExporter::exportPlugins (const Request& request)
{
    Result result;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    const auto& info = request.info;
    const auto& gui = request.gui;

    if (request.formats.isEmpty())
    {
        result.log = "No plugin format was chosen.";
        return result;
    }

    const auto compiler = compilerCommand();
    const auto clapHeaders = findClapHeaders();
    const auto vst3Headers = findVst3Headers();

    if (compiler.isEmpty() || clapHeaders == juce::File() || vst3Headers == juce::File())
    {
        result.log = compilerProblem();
        return result;
    }

    // The work folder is Sonify's own, outside the project.
    const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Sonify")
                          .getChildFile ("export-" + juce::File::createLegalFileName (info.uuid));

    work.deleteRecursively();

    if (! work.createDirectory())
    {
        result.log = "Couldn't create the build folder " + work.getFullPathName();
        return result;
    }

    juce::String graphSource;

    if (const auto graph = WasmCompiler::generateGraphSource (request.projectFolder.getChildFile (WasmCompiler::graphFileName), graphSource); graph.failed())
    {
        result.log = graph.getErrorMessage();
        return result;
    }

    // Protected (the Protection tab): its product id and demo times built in (stella_product.h),
    // its license.soni with it, its registration badge kept.
    const auto protection = Licences::loadProtection (request.projectFolder);
    const bool isProtected = protection.isValid();
    const auto soni = isProtected ? Licences::soniFor (protection, info.name, info.vendor) : stella::licence::Soni();

    // The VST3 class id: the project's own id, fixed when the project was made.
    juce::String vst3Id;
    const auto uuidHex = info.uuid.removeCharacters ("-{}").paddedLeft ('0', 32).substring (0, 32);

    for (int i = 0; i < 16; ++i)
        vst3Id << (i > 0 ? ", " : "") << "0x" << uuidHex.substring (i * 2, i * 2 + 2);

    // An Audio Unit's codes: its kind, and the plugin's own and its maker's (letters and digits;
    // the maker's starts with a capital, as Apple asks).
    const auto kind = info.kind == PluginKind::effect ? 1 : (info.kind == PluginKind::widget ? 2 : 0);
    const juce::String auType = kind == 0 ? "aumu" : (kind == 1 ? "aufx" : "aumf");
    const auto auSubtype = fourCharCode (fnv (info.pluginId.isNotEmpty() ? info.pluginId : info.uuid));
    const auto vendorFirst = info.vendor.toUpperCase().retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const auto auManufacturer = fourCharCode (fnv (info.vendor.toLowerCase()), vendorFirst.isNotEmpty() ? vendorFirst[0] : (juce::juce_wchar) 'S');

    // What the plugin calls itself. A MIDI effect is built as an instrument (plugins don't send MIDI).
    juce::String pluginInfo;
    pluginInfo << "// Generated by Sonify's Plugins Creator Studio for this export.\n#pragma once\n\nnamespace stella::info\n{\n"
               << "    constexpr const char* name = " << textLiteral (info.name) << ";\n"       // UTF-8: a name in any language
               << "    constexpr const char* vendor = " << textLiteral (info.vendor) << ";\n"
               << "    constexpr const char* id = " << literal (info.pluginId) << ";\n"
               << "    constexpr const char* version = " << literal (info.version) << ";\n"
               << "    constexpr const char* description = \"Made with Sonify\";\n"
               << "    constexpr int kind = " << kind << ";   // 0 instrument, 1 effect, 2 widget (audio and notes in)\n"
               << "    constexpr bool isInstrument = kind == 0;\n"
               << "    constexpr unsigned char vst3Id[16] { " << vst3Id << " };\n"
               << "    constexpr unsigned int auType = " << hexOf (auType) << ", auSubtype = " << hexOf (auSubtype)
               << ", auManufacturer = " << hexOf (auManufacturer) << ";\n";

    // A sampler's library: each instrument's name and its blob's file name (found as the plugin starts).
    pluginInfo << "\n    struct LibraryInstrument { const char* name; const char* file; };\n"
               << "    inline constexpr LibraryInstrument library[] {";

    for (const auto& instrument : request.library)
        pluginInfo << " { " << textLiteral (instrument.name) << ", " << textLiteral (instrument.file.getFileName()) << " },";

    if (request.library.isEmpty())
        pluginInfo << " { \"\", \"\" }";

    pluginInfo << " };\n    constexpr int numLibrary = " << request.library.size() << ";\n}\n";

    auto writeData = [&work] (const char* name, const char* data, int size)
    {
        return work.getChildFile (name).replaceWithData (data, (size_t) size);
    };

    if (! WasmCompiler::writeRuntime (work)
        || ! writeData ("stella_keys.h", StellaRuntimeData::stella_keys_h, StellaRuntimeData::stella_keys_hSize)
        || ! writeData ("stella_gui.h", StellaRuntimeData::stella_gui_h, StellaRuntimeData::stella_gui_hSize)
        || ! writeData ("stella_gui.cpp", StellaRuntimeData::stella_gui_cpp, StellaRuntimeData::stella_gui_cppSize)
        || ! writeData ("stella_plugin.h", StellaRuntimeData::stella_plugin_h, StellaRuntimeData::stella_plugin_hSize)
        || ! writeData ("stb_image.h", StellaRuntimeData::stb_image_h, StellaRuntimeData::stb_image_hSize)
        || ! writeData ("stella_clap.cpp", StellaRuntimeData::stella_clap_cpp, StellaRuntimeData::stella_clap_cppSize)
        || ! writeData ("stella_vst3.cpp", StellaRuntimeData::stella_vst3_cpp, StellaRuntimeData::stella_vst3_cppSize)
        || ! writeData ("stella_au.cpp", StellaRuntimeData::stella_au_cpp, StellaRuntimeData::stella_au_cppSize)
        || ! writeData ("stella_gui_win32.cpp", StellaRuntimeData::stella_gui_win32_cpp, StellaRuntimeData::stella_gui_win32_cppSize)
        || ! writeData ("stella_gui_mac.mm", StellaRuntimeData::stella_gui_mac_mm, StellaRuntimeData::stella_gui_mac_mmSize)
        || ! writeData ("stella_gui_x11.cpp", StellaRuntimeData::stella_gui_x11_cpp, StellaRuntimeData::stella_gui_x11_cppSize)
        || ! work.getChildFile ("stella_gui_data.cpp").replaceWithText (guiSource (gui, isProtected))
        || ! work.getChildFile ("stella_graph.cpp").replaceWithText (graphSource)
        || ! work.getChildFile ("stella_plugin_info.h").replaceWithText (pluginInfo))
    {
        result.log = "Couldn't write into the build folder " + work.getFullPathName();
        return result;
    }

    if (isProtected)
    {
        // Seconds as a sum, so no locale's decimal comma can get in.
        auto seconds = [] (double value)
        {
            return "(" + juce::String (juce::roundToInt (juce::jlimit (0.0, 3600.0, value) * 1000.0)) + " / 1000.0)";
        };

        const auto product = "// Generated by Sonify's Plugins Creator Studio for this export: the protected product it is.\n"
                             "#define STELLA_PRODUCT " + literal (protection.product) + "\n"
                             "#define STELLA_DEMO_PLAY " + seconds (protection.playSeconds) + "      // seconds of sound\n"
                             "#define STELLA_DEMO_SILENT " + seconds (protection.silentSeconds) + "    // seconds of silence\n";

        if (! work.getChildFile ("stella_product.h").replaceWithText (product))
        {
            result.log = "Couldn't write into the build folder " + work.getFullPathName();
            return result;
        }
    }

    const auto modules = request.projectFolder.getChildFile ("modules");
    auto moduleFiles = modules.findChildFiles (juce::File::findFiles, false, "*.cpp");
    moduleFiles.sort();

    // Programmed elements: their runtime (drawing, text, the font) and their files, when the
    // window uses any.
    const auto elements = request.projectFolder.getChildFile (WasmCompiler::elementsFolderName);
    auto elementFiles = elements.findChildFiles (juce::File::findFiles, false, "*.cpp");
    elementFiles.sort();
    const bool withElements = gui.numCustom > 0;

    if (withElements)
    {
        juce::StringArray defined, missing;

        for (const auto& element : WasmCompiler::findElements (request.projectFolder))
            defined.add (element.name);

        for (const auto& element : WasmCompiler::builtInElements())
            defined.add (element.name);

        for (const auto& type : gui.elementTypes)
            if (! defined.contains (type))
                missing.add (type.isNotEmpty() ? type : juce::String ("(no name)"));

        if (! missing.isEmpty())
        {
            result.log = "The window uses programmed elements that aren't in the project's elements folder: " + missing.joinIntoString (", ")
                       + ". Ask Fanan AI to program them, or take them off the window.";
            return result;
        }
    }

    // The elements' runtime even without elements: the Registration window writes with its font
    // (stb_truetype.h, stella_fonts.h).
    if (! WasmCompiler::writeElementRuntime (work))
    {
        result.log = "Couldn't write into the build folder " + work.getFullPathName();
        return result;
    }

    const auto baseName = juce::File::createLegalFileName (info.name);

   #if JUCE_MAC
    // Each class of a Mac plugin's window carries a name of its own (two exports never mix).
    const auto objcSuffix = "S" + juce::String::toHexString ((juce::int64) fnv (info.pluginId + "|" + info.version + "|"
                                                                             + juce::String (juce::Time::currentTimeMillis())));
   #endif

    // Where each format's binary is built.
    auto builtFile = [&] (Format format)
    {
       #if JUCE_MAC
        return work.getChildFile ("bundles").getChildFile (fileNameFor (format, info.name)).getChildFile ("Contents").getChildFile ("MacOS").getChildFile (baseName);
       #else
        return work.getChildFile (baseName + (format == Format::clap ? ".clap" : ".vst3.bin"));
       #endif
    };

    auto commandFor = [&] (Format format)
    {
        juce::StringArray args (compiler);
        args.addArray ({ "-O2", "-std=c++17", "-fvisibility=hidden", "-fno-exceptions", "-fno-rtti", "-DNDEBUG", "-w" });

       #if JUCE_WINDOWS
        args.addArray ({ "-static", "-shared" });
       #elif JUCE_MAC
        args.addArray ({ "-bundle", "-arch", "arm64", "-arch", "x86_64", "-mmacosx-version-min=10.13" });
        args.add ("-DSTELLA_OBJC_SUFFIX=" + objcSuffix);

        if (format == Format::au)
            args.add ("-DSTELLA_AU=1");
       #else
        args.addArray ({ "-shared", "-fPIC", "-static-libstdc++", "-static-libgcc" });
       #endif

        args.add ("-I" + work.getFullPathName());
        args.add ("-I" + modules.getFullPathName());

        // The computer's side of the protection (its ID, the key files, license.soni): a
        // protected plugin's registration, and the protected sample libraries any plugin plays.
        args.add ("-DSTELLA_LICENCE_NATIVE=1");

        if (isProtected)
            args.add ("-DSTELLA_PROTECTED=1");

        if (withElements)
        {
            args.add ("-DSTELLA_ELEMENTS=1");
            args.add ("-I" + elements.getFullPathName());
        }

        if (format == Format::vst3)
            args.add ("-I" + vst3Headers.getFullPathName());
        else if (format == Format::clap)
            args.add ("-I" + clapHeaders.getFullPathName());

        args.add ("-o");
        args.add (builtFile (format).getFullPathName());

       #if JUCE_WINDOWS
        const char* windowFile = "stella_gui_win32.cpp";
       #elif JUCE_MAC
        const char* windowFile = "stella_gui_mac.mm";
       #else
        const char* windowFile = "stella_gui_x11.cpp";
       #endif

        for (const auto* source : { "stella_runtime.cpp", "stella_graph.cpp", "stella_gui.cpp", windowFile, "stella_gui_data.cpp" })
            args.add (work.getChildFile (source).getFullPathName());

        args.add (work.getChildFile (format == Format::vst3 ? "stella_vst3.cpp" : (format == Format::clap ? "stella_clap.cpp" : "stella_au.cpp")).getFullPathName());

        if (format == Format::vst3)
            args.add (vst3Headers.getChildFile ("pluginterfaces").getChildFile ("base").getChildFile ("funknown.cpp").getFullPathName());

        for (const auto& file : moduleFiles)
            args.add (file.getFullPathName());

        if (withElements)
        {
            for (const auto* source : { "stella_elements.cpp", "stella_builtin_elements.cpp", "stella_builtin_3d.cpp", "stella_builtin_paint.cpp" })
                args.add (work.getChildFile (source).getFullPathName());

            for (const auto& file : elementFiles)
                args.add (file.getFullPathName());
        }

       #if JUCE_WINDOWS
        args.addArray ({ "-Wl,--strip-all", "-luser32", "-lgdi32", "-lole32", "-lshell32", "-ladvapi32" });   // advapi32: the computer's ID
       #elif JUCE_MAC
        args.addArray ({ "-Wl,-dead_strip", "-Wl,-x", "-framework", "Cocoa", "-framework", "CoreFoundation" });

        if (format == Format::au)
            args.addArray ({ "-framework", "AudioToolbox", "-framework", "CoreAudio" });
       #else
        args.addArray ({ "-Wl,--no-undefined", "-s", "-ldl", "-lpthread" });
       #endif

        return args;
    };

    // Every format at once, each compiler's messages read as they come (so none waits on another).
    struct Job
    {
        Format format;
        juce::StringArray command;
        juce::String log;
        bool started = false, ok = false;
    };

    std::vector<Job> jobs;

    for (const auto format : request.formats)
    {
       #if JUCE_MAC
        builtFile (format).getParentDirectory().createDirectory();
       #else
        if (format == Format::au)
            continue;
       #endif

        jobs.push_back ({ format, commandFor (format), {}, false, false });
    }

    auto tidy = [&] (juce::String log)
    {
        return log.replace (modules.getFullPathName() + juce::File::getSeparatorString(), "modules/")
                  .replace (elements.getFullPathName() + juce::File::getSeparatorString(), "elements/")
                  .replace (work.getFullPathName() + juce::File::getSeparatorString(), "")
                  .trim();
    };

    {
        std::vector<std::thread> threads;

        for (auto& job : jobs)
        {
            threads.emplace_back ([&job, &builtFile, &tidy]
            {
                juce::ChildProcess process;
                job.started = process.start (job.command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr);

                if (! job.started)
                    return;

                job.log = tidy (process.readAllProcessOutput());
                process.waitForProcessToFinish (10 * 60 * 1000);
                job.ok = process.getExitCode() == 0 && builtFile (job.format).existsAsFile();
            });
        }

        for (auto& thread : threads)
            thread.join();
    }

    bool allOk = true;

    for (const auto& job : jobs)
    {
        if (! job.started)
        {
            result.log = "The plugin compiler couldn't be started.";
            return result;
        }

        allOk = allOk && job.ok;

        if (! job.ok || job.log.isNotEmpty())
        {
            // The same messages for each format are said once.
            bool said = false;

            for (const auto& other : jobs)
                said = said || (&other < &job && other.log == job.log);

            if (! said && job.log.isNotEmpty())
                result.log << (result.log.isNotEmpty() ? "\n\n" : "") << formatName (job.format) << ":\n" << job.log;
        }
    }

    result.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;

    if (! allOk)
    {
        if (result.log.isEmpty())
            result.log = "The plugin compiler stopped without saying why.";

        return result;
    }

    // Into the exports folder. A DAW holding an old file locked makes this fail: say so.
    request.destination.createDirectory();

    auto couldNotWrite = [&] (const juce::File& where)
    {
        result.log = (result.log.isNotEmpty() ? result.log + "\n\n" : juce::String())
                   + "Couldn't write into " + where.getFullPathName() + ". If a DAW has the plugin loaded, close it and export again.";
        return result;
    };

    // Each plugin in a folder of its own, by format: VST3\<name>\<name>.vst3, CLAP\<name>\
    // <name>.clap, with its license.soni beside it when it's protected. Only those two are
    // replaced there: anything else (a sample library's blobs) stays. An Audio Unit carries its
    // license.soni inside (Contents/Resources), and sits straight in AU (and in Components).
    auto formatFolder = [&] (Format format)
    {
        return request.destination.getChildFile (format == Format::vst3 ? "VST3" : (format == Format::clap ? "CLAP" : "AU"));
    };

    auto placeInto = [&] (const juce::File& plugin, const juce::File& folder, bool withSoni)
    {
        const auto target = folder.getChildFile (plugin.getFileName());

        if (! folder.createDirectory())
            return juce::File();

        if (target.exists() && ! (target.isDirectory() ? target.deleteRecursively() : target.deleteFile()))
            return juce::File();

        if (! (plugin.isDirectory() ? plugin.copyDirectoryTo (target) : plugin.copyFileTo (target)))
            return juce::File();

        if (withSoni && isProtected && ! Licences::writeSoni (folder.getChildFile (stella::licence::fileName), soni))
            return juce::File();

        return target;
    };

   #if JUCE_MAC
    // Each a bundle: its Info.plist (an Audio Unit's names its factory), then signed for this Mac.
    auto sign = [] (const juce::File& bundle)
    {
        int code = -1;
        const auto output = run ({ "/usr/bin/codesign", "--force", "--sign", "-", "--timestamp=none", bundle.getFullPathName() }, code, 120000);
        return code == 0 ? juce::String() : output.trim();
    };
   #endif

    for (const auto& job : jobs)
    {
        // The plugin as built: a bundle folder (a VST3, anything on a Mac), or the .clap file.
       #if JUCE_MAC
        const auto plugin = work.getChildFile ("bundles").getChildFile (fileNameFor (job.format, info.name));
        const auto ending = job.format == Format::vst3 ? "vst3" : (job.format == Format::clap ? "clap" : "component");
        plugin.getChildFile ("Contents").getChildFile ("Info.plist")
              .replaceWithText (infoPlist (info, baseName, ending, job.format == Format::au ? auType : juce::String(), auSubtype, auManufacturer));
        plugin.getChildFile ("Contents").getChildFile ("PkgInfo").replaceWithText ("BNDL????");

        // An Audio Unit's license.soni inside it (before it's signed).
        if (job.format == Format::au && isProtected)
        {
            const auto resources = plugin.getChildFile ("Contents").getChildFile ("Resources");

            if (! resources.createDirectory() || ! Licences::writeSoni (resources.getChildFile (stella::licence::fileName), soni))
                return couldNotWrite (resources);
        }
       #else
        auto plugin = builtFile (job.format);

        if (job.format == Format::vst3)
        {
            // A VST3 bundle folder: <name>.vst3/Contents/<architecture>/<name>.vst3 (or .so).
            plugin = work.getChildFile ("bundle").getChildFile (baseName + ".vst3");
            plugin.deleteRecursively();

           #if JUCE_WINDOWS
            const auto binary = plugin.getChildFile ("Contents").getChildFile ("x86_64-win").getChildFile (baseName + ".vst3");
           #else
            const auto binary = plugin.getChildFile ("Contents").getChildFile (linuxArchitecture()).getChildFile (baseName + ".so");
           #endif

            if (! binary.getParentDirectory().createDirectory() || ! builtFile (job.format).copyFileTo (binary))
                return couldNotWrite (work);
        }
       #endif

        const bool ownFolder = job.format != Format::au;
        const auto folder = ownFolder ? formatFolder (job.format).getChildFile (baseName) : formatFolder (job.format);
        const auto target = placeInto (plugin, folder, ownFolder);

        if (target == juce::File())
            return couldNotWrite (folder);

       #if JUCE_MAC
        if (const auto problem = sign (target); problem.isNotEmpty())
        {
            result.log = "Couldn't sign " + target.getFileName() + " for this Mac:\n" + problem;
            return result;
        }
       #endif

        result.files.add (target);

        if (request.install)
        {
            const auto installFolderNow = ownFolder ? installFolder (job.format).getChildFile (baseName) : installFolder (job.format);
            const auto installed = placeInto (target, installFolderNow, ownFolder);

            if (installed == juce::File())
                return couldNotWrite (installFolderNow);

           #if JUCE_MAC
            sign (installed);
           #endif

            result.installed.add (installed.getFullPathName());
        }
    }

    // On this computer the plugin finds the creator's own blobs: they're listed for it.
    juce::StringArray blobs;

    for (const auto& instrument : request.library)
        if (instrument.file.existsAsFile())
            blobs.add (instrument.file.getFullPathName());

    if (! blobs.isEmpty())
        PluginLibrary::addToList (info.vendor, info.name, blobs);

    result.folder = request.destination;
    result.isProtected = isProtected;
    result.ok = true;
    return result;
}
