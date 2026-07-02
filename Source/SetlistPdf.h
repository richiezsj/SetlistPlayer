#pragma once
#include <JuceHeader.h>
#include <string>
#include <vector>
#include "Project.h"

// ===========================================================================
// Minimal, dependency-free PDF export for a setlist.
//
// JUCE has no public PDF writer, so this emits a valid PDF 1.4 document by
// hand: one or more A4 pages of text using the standard Helvetica fonts (no
// embedding). Header-only so no new compilation unit is needed in the .jucer.
// ===========================================================================
namespace setlistpdf
{
    // Escape + encode a string to a PDF literal payload (Latin-1 / WinAnsi).
    // Returns raw bytes (not a juce::String, which would re-encode as UTF-8).
    inline std::string encodeText(const juce::String& s)
    {
        std::string out;
        for (auto it = s.begin(); it != s.end(); ++it)
        {
            const juce::juce_wchar c = *it;
            if (c == '\\' || c == '(' || c == ')') { out += '\\'; out += (char) c; }
            else if (c >= 32 && c < 127)            out += (char) c;
            else if (c >= 160 && c < 256)           out += (char) (unsigned char) c; // Latin-1
            // control chars and unmapped code points are dropped
        }
        return out;
    }

    struct Line { juce::String text; bool bold = false; float size = 11.0f; float indent = 0.0f; };

    // Split notes into wrapped lines at a rough character budget.
    inline void appendWrapped(std::vector<Line>& lines, const juce::String& text,
                              int maxChars, float size, float indent)
    {
        for (auto rawLine : juce::StringArray::fromLines(text))
        {
            rawLine = rawLine.trimEnd();
            while (rawLine.length() > maxChars)
            {
                int cut = maxChars;
                int space = rawLine.substring(0, maxChars).lastIndexOfChar(' ');
                if (space > maxChars / 2) cut = space;
                lines.push_back({ rawLine.substring(0, cut).trim(), false, size, indent });
                rawLine = rawLine.substring(cut).trim();
            }
            lines.push_back({ rawLine, false, size, indent });
        }
    }

    inline std::vector<Line> buildLines(const Project& project)
    {
        std::vector<Line> lines;
        lines.push_back({ project.projectName.isNotEmpty() ? project.projectName : "Setlist",
                          true, 20.0f, 0.0f });
        lines.push_back({ juce::String(project.songs.size()) + " songs", false, 10.0f, 0.0f });
        lines.push_back({ "", false, 6.0f, 0.0f });

        for (int i = 0; i < project.songs.size(); ++i)
        {
            const auto& s = project.songs.getReference(i);
            lines.push_back({ juce::String(i + 1) + ".  " + s.name, true, 13.0f, 0.0f });

            juce::StringArray meta;
            meta.add(juce::String(s.bpm, 1) + " BPM");
            meta.add(juce::String(s.beatsPerBar) + "/" + juce::String(s.beatUnit));
            if (s.key.isNotEmpty())               meta.add("Key: " + s.key);
            if (s.audioFile.getFileName().isNotEmpty()) meta.add(s.audioFile.getFileName());
            lines.push_back({ meta.joinIntoString("   \xc2\xb7   "), false, 10.0f, 16.0f });

            if (s.notes.isNotEmpty())
                appendWrapped(lines, s.notes, 90, 9.5f, 16.0f);

            lines.push_back({ "", false, 6.0f, 0.0f });
        }
        return lines;
    }

    inline bool write(const Project& project, const juce::File& file)
    {
        constexpr float pageW = 595.0f, pageH = 842.0f;
        constexpr float marginL = 50.0f, marginBottom = 50.0f, pageTop = pageH - 60.0f;

        const auto lines = buildLines(project);

        // --- lay out lines into page content streams ---
        std::vector<std::string> pages;
        std::string current;
        float y = pageTop;
        auto flush = [&] { pages.push_back(current); current.clear(); };

        for (const auto& ln : lines)
        {
            const float leading = ln.size * 1.5f;
            if (y - leading < marginBottom) { flush(); y = pageTop; }
            y -= leading;

            if (ln.text.isNotEmpty())
            {
                const std::string font = ln.bold ? "F2" : "F1";
                current += "BT /" + font + " " + juce::String(ln.size, 1).toStdString() + " Tf ";
                current += juce::String(marginL + ln.indent, 1).toStdString() + " "
                         + juce::String(y, 1).toStdString() + " Td (";
                current += encodeText(ln.text);
                current += ") Tj ET\n";
            }
        }
        flush();
        if (pages.empty()) pages.push_back("");

        // --- assemble the PDF objects ---
        const int numPages = (int) pages.size();
        const int numObjects = 4 + 2 * numPages;   // catalog, pages, 2 fonts, then page+content pairs
        std::vector<size_t> offset((size_t) numObjects + 1, 0);

        juce::MemoryOutputStream mos;
        auto put = [&](const std::string& s) { mos.write(s.data(), s.size()); };
        auto beginObj = [&](int n) { offset[(size_t) n] = (size_t) mos.getPosition();
                                     put(std::to_string(n) + " 0 obj\n"); };
        auto endObj = [&] { put("endobj\n"); };

        put("%PDF-1.4\n");

        // 1: catalog, 2: pages tree
        beginObj(1); put("<< /Type /Catalog /Pages 2 0 R >>\n"); endObj();

        std::string kids;
        for (int p = 0; p < numPages; ++p)
            kids += std::to_string(5 + 2 * p) + " 0 R ";
        beginObj(2);
        put("<< /Type /Pages /Kids [ " + kids + "] /Count " + std::to_string(numPages) + " >>\n");
        endObj();

        // 3,4: standard fonts
        beginObj(3); put("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
                         "/Encoding /WinAnsiEncoding >>\n"); endObj();
        beginObj(4); put("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold "
                         "/Encoding /WinAnsiEncoding >>\n"); endObj();

        for (int p = 0; p < numPages; ++p)
        {
            const int pageObj = 5 + 2 * p;
            const int contentObj = 6 + 2 * p;

            beginObj(pageObj);
            put("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
                + juce::String(pageW, 0).toStdString() + " " + juce::String(pageH, 0).toStdString()
                + "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents "
                + std::to_string(contentObj) + " 0 R >>\n");
            endObj();

            beginObj(contentObj);
            put("<< /Length " + std::to_string(pages[(size_t) p].size()) + " >>\nstream\n");
            put(pages[(size_t) p]);
            put("\nendstream\n");
            endObj();
        }

        // --- cross-reference table + trailer ---
        const size_t xrefPos = (size_t) mos.getPosition();
        put("xref\n0 " + std::to_string(numObjects + 1) + "\n");
        put("0000000000 65535 f \n");
        for (int n = 1; n <= numObjects; ++n)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", offset[(size_t) n]);
            put(buf);
        }
        put("trailer << /Size " + std::to_string(numObjects + 1) + " /Root 1 0 R >>\n");
        put("startxref\n" + std::to_string(xrefPos) + "\n%%EOF\n");

        // --- write to disk ---
        if (auto stream = file.createOutputStream())
        {
            stream->setPosition(0);
            stream->truncate();
            stream->write(mos.getData(), mos.getDataSize());
            return true;
        }
        return false;
    }
}
