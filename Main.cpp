#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winmm.lib")

// ============================================================
// CONSTANTS
// ============================================================

constexpr int NUM_KEYS = 88;
constexpr int FIRST_MIDI = 21;
constexpr int LAST_MIDI = 108;

constexpr int SAMPLE_RATE = 44100;
constexpr double NOTE_LENGTH = 0.8;
constexpr double VOLUME_BOOST = 5.0;

// ============================================================
// GLOBALS
// ============================================================

HWND mainWindow = nullptr;

HWND instrumentButton = nullptr;
HWND clearButton = nullptr;
HWND copyButton = nullptr;
HWND outputEditor = nullptr;

int selectedInstrument = 0;

struct PianoKey
{
    int midiNote;
    double frequency;

    RECT rect;

    bool black;
    bool pressed;
};

std::vector<PianoKey> keys;

// ============================================================
// INSTRUMENT NAMES
// ============================================================

const char* instrumentNames[128] =
{
    "PIANO",
    "BRIGHTPIANO",
    "ELECTRICGRAND",
    "HONKYTONK",
    "EPIANO1",
    "EPIANO2",
    "HARPSICHORD",
    "CLAVINET",

    "CELESTA",
    "GLOCKENSPIEL",
    "MUSICBOX",
    "VIBRAPHONE",
    "MARIMBA",
    "XYLOPHONE",
    "TUBULARBELLS",
    "DULCIMER",

    "ORGAN",
    "PERCORGAN",
    "ROCKORGAN",
    "CHURCHORGAN",
    "REEDORGAN",
    "ACCORDION",
    "HARMONICA",
    "TANGOACCORDION",

    "NYLONGUITAR",
    "STEELGUITAR",
    "JAZZGUITAR",
    "CLEANGUITAR",
    "MUTEDGUITAR",
    "OVERDRIVE",
    "DISTORTION",
    "GUITARHARMONICS",

    "ACOUSTICBASS",
    "FINGERBASS",
    "PICKBASS",
    "FRETLESSBASS",
    "SLAPBASS1",
    "SLAPBASS2",
    "SYNTHBASS1",
    "SYNTHBASS2",

    "VIOLIN",
    "VIOLA",
    "CELLO",
    "CONTRABASS",
    "TREMOLOSTRINGS",
    "PIZZICATO",
    "HARP",
    "TIMPANI",

    "STRINGENSEMBLE1",
    "STRINGENSEMBLE2",
    "SYNTHSTRINGS1",
    "SYNTHSTRINGS2",
    "CHOIR",
    "VOICE",
    "SYNTHCHOIR",
    "ORCHESTRAHIT",

    "TRUMPET",
    "TROMBONE",
    "TUBA",
    "MUTEDTRUMPET",
    "FRENCHHORN",
    "BRASS",
    "SYNTHBRASS1",
    "SYNTHBRASS2",

    "SOPRANOSAX",
    "ALTOSAX",
    "TENORSAX",
    "BARITONESAX",
    "OBOE",
    "ENGLISHHORN",
    "BASSOON",
    "CLARINET",

    "PICCOLO",
    "FLUTE",
    "RECORDER",
    "PANFLUTE",
    "BLOWNBOTTLE",
    "SHAKUHACHI",
    "WHISTLE",
    "OCARINA",

    "LEADSQUARE",
    "LEADSAW",
    "LEADCALLIOPE",
    "LEADCHIFF",
    "LEADCHARANG",
    "LEADVOICE",
    "LEADFIFTHS",
    "LEADBASS",

    "PADNEWAGE",
    "PADWARM",
    "PADPOLYSYNTH",
    "PADCHOIR",
    "PADBOWED",
    "PADMETALLIC",
    "PADHALO",
    "PADSWEEP",

    "FXRAIN",
    "FXSOUNDTRACK",
    "FXCRYSTAL",
    "FXATMOSPHERE",
    "FXBRIGHTNESS",
    "FXGOBLINS",
    "FXECHOES",
    "FXSCIFI",

    "SITAR",
    "BANJO",
    "SHAMISEN",
    "KOTO",
    "KALIMBA",
    "BAGPIPE",
    "FIDDLE",
    "SHANAI",

    "TINKLEBELL",
    "AGOGO",
    "STEELDRUMS",
    "WOODBLOCK",
    "TAIKODRUM",
    "MELODIC TOM",
    "SYNTHDRUM",
    "REVERSECYMBAL",

    "GUITARFRETNOISE",
    "BREATHNOISE",
    "SEASHORE",
    "BIRDTWEET",
    "TELEPHONERING",
    "HELICOPTER",
    "APPLAUSE",
    "GUNSHOT"
};

// ============================================================
// NOTE NAMES
// ============================================================

const char* noteNames[12] =
{
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B"
};

// ============================================================
// MIDI TO FREQUENCY
// ============================================================

double MidiToFrequency(int midi)
{
    return 440.0 *
        std::pow(
            2.0,
            (static_cast<double>(midi) - 69.0) / 12.0
        );
}

// ============================================================
// MIDI NOTE NAME
// ============================================================

std::string GetNoteName(int midi)
{
    int noteIndex = midi % 12;

    int octave =
        (midi / 12) - 1;

    std::string result =
        noteNames[noteIndex];

    result +=
        std::to_string(octave);

    return result;
}

// ============================================================
// IS BLACK KEY
// ============================================================

bool IsBlackKey(int midi)
{
    int note = midi % 12;

    return
        note == 1 ||
        note == 3 ||
        note == 6 ||
        note == 8 ||
        note == 10;
}

// ============================================================
// CREATE 88 KEYS
// ============================================================

void CreatePiano()
{
    keys.clear();

    for (int midi = FIRST_MIDI;
         midi <= LAST_MIDI;
         ++midi)
    {
        PianoKey key{};

        key.midiNote = midi;
        key.frequency = MidiToFrequency(midi);
        key.black = IsBlackKey(midi);
        key.pressed = false;

        keys.push_back(key);
    }
}

// ============================================================
// LAYOUT ONE ROW
//
// This is the important part for the black-key positions.
//
// White keys are laid out first.
// Black keys are then placed exactly on the boundaries
// between their neighboring white keys.
// ============================================================

void LayoutRow(
    int firstKey,
    int lastKey,
    int top,
    int bottom,
    int windowWidth)
{
    const int margin = 10;

    int usableWidth =
        windowWidth - margin * 2;

    if (usableWidth < 100)
        usableWidth = 100;

    // --------------------------------------------------------
    // Count white keys
    // --------------------------------------------------------

    int whiteCount = 0;

    for (int i = firstKey;
         i <= lastKey;
         ++i)
    {
        if (!keys[i].black)
            ++whiteCount;
    }

    if (whiteCount == 0)
        return;

    double whiteWidth =
        static_cast<double>(usableWidth) /
        static_cast<double>(whiteCount);

    // --------------------------------------------------------
    // Position white keys
    // --------------------------------------------------------

    int whiteIndex = 0;

    for (int i = firstKey;
         i <= lastKey;
         ++i)
    {
        if (keys[i].black)
            continue;

        int left =
            margin +
            static_cast<int>(
                whiteIndex * whiteWidth
            );

        int right =
            margin +
            static_cast<int>(
                (whiteIndex + 1) * whiteWidth
            );

        keys[i].rect.left = left;
        keys[i].rect.right = right;
        keys[i].rect.top = top;
        keys[i].rect.bottom = bottom;

        ++whiteIndex;
    }

    // --------------------------------------------------------
    // Position black keys
    // --------------------------------------------------------

    int blackWidth =
        std::max(
            10,
            static_cast<int>(
                whiteWidth * 0.60
            )
        );

    int blackHeight =
        static_cast<int>(
            (bottom - top) * 0.60
        );

    for (int i = firstKey;
         i <= lastKey;
         ++i)
    {
        if (!keys[i].black)
            continue;

        // Find the white key immediately before this
        // black key.
        int previousWhite = -1;

        for (int j = i - 1;
             j >= firstKey;
             --j)
        {
            if (!keys[j].black)
            {
                previousWhite = j;
                break;
            }
        }

        if (previousWhite == -1)
            continue;

        // The boundary between the two white keys is
        // exactly where the black key belongs.
        int boundary =
            keys[previousWhite].rect.right;

        keys[i].rect.left =
            boundary -
            blackWidth / 2;

        keys[i].rect.right =
            boundary +
            blackWidth / 2;

        keys[i].rect.top =
            top;

        keys[i].rect.bottom =
            top +
            blackHeight;
    }
}

// ============================================================
// LAYOUT ALL 88 KEYS
// ============================================================

void LayoutPiano(
    int width,
    int height)
{
    if (keys.size() != NUM_KEYS)
        return;

    const int pianoTop = 155;
    const int bottomMargin = 10;

    int pianoBottom =
        height - bottomMargin;

    if (pianoBottom <= pianoTop)
        return;

    int totalHeight =
        pianoBottom - pianoTop;

    int rowHeight =
        totalHeight / 2;

    // 44 keys per row.
    //
    // Row 1:
    // A0 through C4
    //
    // Row 2:
    // C#4 through C8
    //
    // This is not ideal musically because the split occurs
    // at C#4, so instead we use an exact 44/44 split.
    //
    // The black keys are still correctly placed relative to
    // the neighboring white keys in each row.

    LayoutRow(
        0,
        43,
        pianoTop,
        pianoTop + rowHeight,
        width
    );

    LayoutRow(
        44,
        87,
        pianoTop + rowHeight,
        pianoBottom,
        width
    );
}

// ============================================================
// FIND KEY
// ============================================================

PianoKey* FindKeyAt(
    int x,
    int y)
{
    // Black keys first because they sit on top
    // of the white keys.
    for (PianoKey& key : keys)
    {
        if (!key.black)
            continue;

        if (PtInRect(
                &key.rect,
                POINT{ x, y }))
        {
            return &key;
        }
    }

    // Then white keys.
    for (PianoKey& key : keys)
    {
        if (key.black)
            continue;

        if (PtInRect(
                &key.rect,
                POINT{ x, y }))
        {
            return &key;
        }
    }

    return nullptr;
}

// ============================================================
// WAVEFORM
// ============================================================

double MakeWave(
    double phase,
    int instrument)
{
    double sine =
        std::sin(phase);

    double saw =
        2.0 *
        (phase /
         (2.0 * 3.14159265358979323846))
        - 1.0;

    double square =
        sine >= 0.0
        ? 1.0
        : -1.0;

    int family =
        instrument / 8;

    switch (family)
    {
        case 0:
            // Piano
            return
                0.75 * sine +
                0.18 * std::sin(phase * 2.0) +
                0.07 * std::sin(phase * 3.0);

        case 1:
            // Bells/percussion
            return
                0.60 * sine +
                0.25 * std::sin(phase * 3.0) +
                0.15 * std::sin(phase * 5.0);

        case 2:
            // Organ
            return
                0.45 * sine +
                0.25 * std::sin(phase * 2.0) +
                0.18 * std::sin(phase * 3.0) +
                0.12 * std::sin(phase * 4.0);

        case 3:
            // Guitar
            return
                0.65 * sine +
                0.35 * saw;

        case 4:
            // Bass
            return
                0.80 * sine +
                0.20 * std::sin(phase * 2.0);

        case 5:
            // Strings
            return
                0.60 * sine +
                0.20 * std::sin(phase * 2.0) +
                0.20 * std::sin(phase * 3.0);

        case 6:
            // Brass
            return
                0.40 * sine +
                0.35 * saw +
                0.25 * std::sin(phase * 3.0);

        case 7:
            // Woodwinds
            return
                0.70 * sine +
                0.20 * std::sin(phase * 2.0) +
                0.10 * std::sin(phase * 4.0);

        case 8:
            // Synth lead
            return
                0.50 * saw +
                0.50 * square;

        case 9:
            // Pads
            return
                0.70 * sine +
                0.15 * std::sin(phase * 2.0) +
                0.15 * std::sin(phase * 4.0);

        case 10:
            // Effects
            return
                0.55 * sine +
                0.25 * saw +
                0.20 * std::sin(phase * 7.0);

        case 11:
            // Ethnic
            return
                0.55 * sine +
                0.45 * saw;

        case 12:
            // Percussion
            return
                0.40 * sine +
                0.60 * square;

        default:
            return
                0.70 * sine +
                0.30 * saw;
    }
}

// ============================================================
// PLAY SOUND
// ============================================================

void PlayNoteSound(
    double frequency,
    int instrument)
{
    std::thread(
        [frequency, instrument]()
        {
            const int sampleCount =
                static_cast<int>(
                    SAMPLE_RATE *
                    NOTE_LENGTH
                );

            std::vector<short> samples(
                sampleCount
            );

            const double twoPi =
                2.0 *
                3.14159265358979323846;

            for (int i = 0;
                 i < sampleCount;
                 ++i)
            {
                double time =
                    static_cast<double>(i) /
                    SAMPLE_RATE;

                double phase =
                    std::fmod(
                        twoPi *
                        frequency *
                        time,
                        twoPi
                    );

                double attack =
                    std::min(
                        1.0,
                        time / 0.015
                    );

                double release =
                    std::max(
                        0.0,
                        1.0 -
                        time / NOTE_LENGTH
                    );

                release *= release;

                double value =
                    MakeWave(
                        phase,
                        instrument
                    );

                value *=
                    attack *
                    release;

                // 5X volume boost.
                value *= VOLUME_BOOST;

                // Prevent digital clipping.
                value =
                    std::max(
                        -1.0,
                        std::min(
                            1.0,
                            value
                        )
                    );

                samples[i] =
                    static_cast<short>(
                        value *
                        32767.0
                    );
            }

            WAVEFORMATEX format{};

            format.wFormatTag =
                WAVE_FORMAT_PCM;

            format.nChannels = 1;

            format.nSamplesPerSec =
                SAMPLE_RATE;

            format.wBitsPerSample = 16;

            format.nBlockAlign =
                format.nChannels *
                format.wBitsPerSample /
                8;

            format.nAvgBytesPerSec =
                format.nSamplesPerSec *
                format.nBlockAlign;

            HWAVEOUT waveOut =
                nullptr;

            MMRESULT result =
                waveOutOpen(
                    &waveOut,
                    WAVE_MAPPER,
                    &format,
                    0,
                    0,
                    CALLBACK_NULL
                );

            if (result != MMSYSERR_NOERROR)
                return;

            WAVEHDR header{};

            header.lpData =
                reinterpret_cast<LPSTR>(
                    samples.data()
                );

            header.dwBufferLength =
                static_cast<DWORD>(
                    samples.size() *
                    sizeof(short)
                );

            result =
                waveOutPrepareHeader(
                    waveOut,
                    &header,
                    sizeof(header)
                );

            if (result == MMSYSERR_NOERROR)
            {
                result =
                    waveOutWrite(
                        waveOut,
                        &header,
                        sizeof(header)
                    );

                if (result == MMSYSERR_NOERROR)
                {
                    Sleep(
                        static_cast<DWORD>(
                            NOTE_LENGTH *
                            1000.0
                        )
                    );
                }

                waveOutUnprepareHeader(
                    waveOut,
                    &header,
                    sizeof(header)
                );
            }

            waveOutReset(
                waveOut
            );

            waveOutClose(
                waveOut
            );
        }
    ).detach();
}

// ============================================================
// APPEND OUTPUT
// ============================================================

void AppendOutput(
    const std::string& text)
{
    if (outputEditor == nullptr)
        return;

    int length =
        GetWindowTextLengthA(
            outputEditor
        );

    SendMessageA(
        outputEditor,
        EM_SETSEL,
        length,
        length
    );

    SendMessageA(
        outputEditor,
        EM_REPLACESEL,
        FALSE,
        reinterpret_cast<LPARAM>(
            text.c_str()
        )
    );
}

// ============================================================
// PRINT PLAYER COMMAND
//
// FORMAT:
//
// FLUTE D#2 0 1
//
// Instrument
// Note
// Start
// End
// ============================================================

void PrintNote(
    const PianoKey& key)
{
    std::ostringstream output;

    output
        << instrumentNames[selectedInstrument]
        << " "
        << GetNoteName(key.midiNote)
        << " 0 1\r\n";

    AppendOutput(
        output.str()
    );
}

// ============================================================
// CLEAR
// ============================================================

void ClearOutput()
{
    if (outputEditor != nullptr)
    {
        SetWindowTextA(
            outputEditor,
            ""
        );
    }
}

// ============================================================
// COPY
// ============================================================

void CopyOutput()
{
    if (outputEditor == nullptr)
        return;

    int length =
        GetWindowTextLengthA(
            outputEditor
        );

    if (length <= 0)
        return;

    std::vector<char> buffer(
        static_cast<size_t>(length) + 1
    );

    GetWindowTextA(
        outputEditor,
        buffer.data(),
        length + 1
    );

    if (!OpenClipboard(mainWindow))
        return;

    EmptyClipboard();

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            buffer.size()
        );

    if (memory != nullptr)
    {
        void* data =
            GlobalLock(memory);

        if (data != nullptr)
        {
            std::memcpy(
                data,
                buffer.data(),
                buffer.size()
            );

            GlobalUnlock(
                memory
            );

            SetClipboardData(
                CF_TEXT,
                memory
            );
        }
        else
        {
            GlobalFree(
                memory
            );
        }
    }

    CloseClipboard();
}

// ============================================================
// UPDATE INSTRUMENT BUTTON
// ============================================================

void UpdateInstrumentButton()
{
    if (instrumentButton == nullptr)
        return;

    std::string text =
        "Instrument: ";

    text +=
        instrumentNames[
            selectedInstrument
        ];

    text +=
        " (" +
        std::to_string(
            selectedInstrument + 1
        ) +
        "/128)";

    SetWindowTextA(
        instrumentButton,
        text.c_str()
    );
}

// ============================================================
// NEXT INSTRUMENT
// ============================================================

void NextInstrument()
{
    ++selectedInstrument;

    if (selectedInstrument >= 128)
        selectedInstrument = 0;

    UpdateInstrumentButton();
}

// ============================================================
// RESIZE
// ============================================================

void ResizeControls()
{
    if (mainWindow == nullptr)
        return;

    RECT rect{};

    GetClientRect(
        mainWindow,
        &rect
    );

    int width =
        rect.right - rect.left;

    // --------------------------------------------------------
    // Instrument
    // --------------------------------------------------------

    if (instrumentButton != nullptr)
    {
        MoveWindow(
            instrumentButton,
            10,
            10,
            std::max(
                250,
                width - 240
            ),
            35,
            TRUE
        );
    }

    // --------------------------------------------------------
    // Clear
    // --------------------------------------------------------

    if (clearButton != nullptr)
    {
        MoveWindow(
            clearButton,
            std::max(
                10,
                width - 220
            ),
            10,
            100,
            35,
            TRUE
        );
    }

    // --------------------------------------------------------
    // Copy
    // --------------------------------------------------------

    if (copyButton != nullptr)
    {
        MoveWindow(
            copyButton,
            std::max(
                10,
                width - 110
            ),
            10,
            100,
            35,
            TRUE
        );
    }

    // --------------------------------------------------------
    // Output
    // --------------------------------------------------------

    if (outputEditor != nullptr)
    {
        MoveWindow(
            outputEditor,
            10,
            50,
            std::max(
                300,
                width - 20
            ),
            95,
            TRUE
        );
    }

    LayoutPiano(
        width,
        rect.bottom
    );

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// DRAW PIANO
// ============================================================

void DrawPiano(
    HDC hdc)
{
    RECT client{};

    GetClientRect(
        mainWindow,
        &client
    );

    HBRUSH background =
        CreateSolidBrush(
            RGB(35, 35, 40)
        );

    FillRect(
        hdc,
        &client,
        background
    );

    DeleteObject(
        background
    );

    // --------------------------------------------------------
    // White keys
    // --------------------------------------------------------

    for (const PianoKey& key : keys)
    {
        if (key.black)
            continue;

        COLORREF color =
            key.pressed
            ? RGB(90, 175, 255)
            : RGB(245, 245, 245);

        HBRUSH brush =
            CreateSolidBrush(
                color
            );

        FillRect(
            hdc,
            &key.rect,
            brush
        );

        DeleteObject(
            brush
        );

        FrameRect(
            hdc,
            &key.rect,
            static_cast<HBRUSH>(
                GetStockObject(
                    BLACK_BRUSH
                )
            )
        );
    }

    // --------------------------------------------------------
    // Black keys
    // --------------------------------------------------------

    for (const PianoKey& key : keys)
    {
        if (!key.black)
            continue;

        COLORREF color =
            key.pressed
            ? RGB(40, 120, 230)
            : RGB(10, 10, 12);

        HBRUSH brush =
            CreateSolidBrush(
                color
            );

        FillRect(
            hdc,
            &key.rect,
            brush
        );

        DeleteObject(
            brush
        );

        FrameRect(
            hdc,
            &key.rect,
            static_cast<HBRUSH>(
                GetStockObject(
                    BLACK_BRUSH
                )
            )
        );
    }

    // --------------------------------------------------------
    // MIDI numbers
    // --------------------------------------------------------

    SetBkMode(
        hdc,
        TRANSPARENT
    );

    SetTextColor(
        hdc,
        RGB(50, 50, 50)
    );

    for (const PianoKey& key : keys)
    {
        if (key.black)
            continue;

        RECT textRect =
            key.rect;

        textRect.top =
            textRect.bottom - 25;

        std::string number =
            std::to_string(
                key.midiNote
            );

        DrawTextA(
            hdc,
            number.c_str(),
            -1,
            &textRect,
            DT_CENTER |
            DT_SINGLELINE |
            DT_BOTTOM
        );
    }
}

// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        // ====================================================
        // CREATE
        // ====================================================

        case WM_CREATE:
        {
            mainWindow =
                window;

            instrumentButton =
                CreateWindowA(
                    "BUTTON",
                    "Instrument",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_PUSHBUTTON,
                    10,
                    10,
                    700,
                    35,
                    window,
                    reinterpret_cast<HMENU>(1001),
                    GetModuleHandle(nullptr),
                    nullptr
                );

            clearButton =
                CreateWindowA(
                    "BUTTON",
                    "Clear",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_PUSHBUTTON,
                    720,
                    10,
                    100,
                    35,
                    window,
                    reinterpret_cast<HMENU>(1002),
                    GetModuleHandle(nullptr),
                    nullptr
                );

            copyButton =
                CreateWindowA(
                    "BUTTON",
                    "Copy",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_PUSHBUTTON,
                    830,
                    10,
                    100,
                    35,
                    window,
                    reinterpret_cast<HMENU>(1003),
                    GetModuleHandle(nullptr),
                    nullptr
                );

            outputEditor =
                CreateWindowExA(
                    WS_EX_CLIENTEDGE,
                    "EDIT",
                    "",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_VSCROLL |
                    ES_MULTILINE |
                    ES_AUTOVSCROLL |
                    ES_READONLY,
                    10,
                    50,
                    900,
                    95,
                    window,
                    reinterpret_cast<HMENU>(1004),
                    GetModuleHandle(nullptr),
                    nullptr
                );

            UpdateInstrumentButton();

            return 0;
        }

        // ====================================================
        // RESIZE
        // ====================================================

        case WM_SIZE:
        {
            ResizeControls();
            return 0;
        }

        // ====================================================
        // BUTTONS
        // ====================================================

        case WM_COMMAND:
        {
            int id =
                LOWORD(wParam);

            if (id == 1001)
            {
                NextInstrument();
                return 0;
            }

            if (id == 1002)
            {
                ClearOutput();
                return 0;
            }

            if (id == 1003)
            {
                CopyOutput();
                return 0;
            }

            return 0;
        }

        // ====================================================
        // LEFT BUTTON DOWN
        // ====================================================

        case WM_LBUTTONDOWN:
        {
            int x =
                static_cast<int>(
                    static_cast<short>(
                        LOWORD(lParam)
                    )
                );

            int y =
                static_cast<int>(
                    static_cast<short>(
                        HIWORD(lParam)
                    )
                );

            PianoKey* key =
                FindKeyAt(
                    x,
                    y
                );

            if (key != nullptr)
            {
                for (PianoKey& current : keys)
                    current.pressed = false;

                key->pressed = true;

                PlayNoteSound(
                    key->frequency,
                    selectedInstrument
                );

                PrintNote(
                    *key
                );

                SetCapture(
                    window
                );

                InvalidateRect(
                    window,
                    nullptr,
                    FALSE
                );
            }

            return 0;
        }

        // ====================================================
        // MOUSE MOVE
        // ====================================================

        case WM_MOUSEMOVE:
        {
            bool leftButtonDown =
                (wParam & MK_LBUTTON) != 0;

            if (!leftButtonDown)
                return 0;

            int x =
                static_cast<int>(
                    static_cast<short>(
                        LOWORD(lParam)
                    )
                );

            int y =
                static_cast<int>(
                    static_cast<short>(
                        HIWORD(lParam)
                    )
                );

            PianoKey* key =
                FindKeyAt(
                    x,
                    y
                );

            if (key != nullptr &&
                !key->pressed)
            {
                for (PianoKey& current : keys)
                    current.pressed = false;

                key->pressed = true;

                PlayNoteSound(
                    key->frequency,
                    selectedInstrument
                );

                PrintNote(
                    *key
                );

                InvalidateRect(
                    window,
                    nullptr,
                    FALSE
                );
            }

            return 0;
        }

        // ====================================================
        // LEFT BUTTON UP
        // ====================================================

        case WM_LBUTTONUP:
        {
            for (PianoKey& key : keys)
                key.pressed = false;

            ReleaseCapture();

            InvalidateRect(
                window,
                nullptr,
                FALSE
            );

            return 0;
        }

        // ====================================================
        // PAINT
        // ====================================================

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};

            HDC hdc =
                BeginPaint(
                    window,
                    &ps
                );

            DrawPiano(
                hdc
            );

            EndPaint(
                window,
                &ps
            );

            return 0;
        }

        // ====================================================
        // DESTROY
        // ====================================================

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(
        window,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// PROGRAM ENTRY
// ============================================================

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int nCmdShow)
{
    CreatePiano();

    const char CLASS_NAME[] =
        "PrintablePiano88";

    WNDCLASSA wc{};

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursor(
            nullptr,
            IDC_ARROW
        );

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1
        );

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(
            nullptr,
            "Failed to register window class.",
            "Error",
            MB_ICONERROR
        );

        return 1;
    }

    HWND window =
        CreateWindowExA(
            0,
            CLASS_NAME,
            "88-Key Music Player Code Keyboard",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1100,
            750,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );

    if (window == nullptr)
    {
        MessageBoxA(
            nullptr,
            "Failed to create window.",
            "Error",
            MB_ICONERROR
        );

        return 1;
    }

    mainWindow =
        window;

    ShowWindow(
        window,
        nCmdShow
    );

    UpdateWindow(
        window
    );

    MSG msg{};

    while (
        GetMessage(
            &msg,
            nullptr,
            0,
            0
        ) > 0
    )
    {
        TranslateMessage(
            &msg
        );

        DispatchMessage(
            &msg
        );
    }

    return static_cast<int>(
        msg.wParam
    );
}
