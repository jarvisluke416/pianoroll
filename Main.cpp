#define UNICODE
#define _UNICODE
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <memory>

#pragma comment(lib, "winmm.lib")

// ============================================================
// SETTINGS
// ============================================================

constexpr int TOTAL_KEYS = 88;

constexpr int SAMPLE_RATE = 44100;
constexpr double NOTE_LENGTH = 0.45;
constexpr double PI = 3.14159265358979323846;

// 5X volume boost requested.
// The final sample is clipped to the valid 16-bit range.
constexpr double VOLUME_BOOST = 5.0;

// ============================================================
// GLOBAL WINDOW
// ============================================================

HWND mainWindow = nullptr;
HWND instrumentButton = nullptr;
HWND clearButton = nullptr;
HWND copyButton = nullptr;
HWND outputEditor = nullptr;

// ============================================================
// INSTRUMENTS
// ============================================================

enum class Instrument
{
    PIANO,
    FLUTE,
    ORGAN,
    BELL,
    STRINGS,
    SYNTH
};

const std::vector<std::wstring> instrumentNames =
{
    L"PIANO",
    L"FLUTE",
    L"ORGAN",
    L"BELL",
    L"STRINGS",
    L"SYNTH"
};

Instrument currentInstrument = Instrument::FLUTE;

// ============================================================
// PIANO KEY
// ============================================================

struct PianoKey
{
    int midiNote = 0;

    std::wstring noteName;
    double frequency = 0.0;

    bool black = false;
    bool pressed = false;

    RECT rect{};
};

std::vector<PianoKey> keys;

// ============================================================
// NOTE POSITION
// ============================================================

// This is the important update.
//
// The first played note is position 0.
// The second is position 1.
// The third is position 2.
// etc.

int notePosition = 0;

// ============================================================
// MIDI NOTE NAMES
// ============================================================

const wchar_t* noteNames[12] =
{
    L"C",
    L"C#",
    L"D",
    L"D#",
    L"E",
    L"F",
    L"F#",
    L"G",
    L"G#",
    L"A",
    L"A#",
    L"B"
};

// ============================================================
// MIDI -> FREQUENCY
// ============================================================

double MidiToFrequency(int midi)
{
    return 440.0 *
        std::pow(
            2.0,
            (midi - 69) / 12.0
        );
}

// ============================================================
// MIDI -> NOTE NAME
// ============================================================

std::wstring MidiToNoteName(int midi)
{
    int octave =
        (midi / 12) - 1;

    int note =
        midi % 12;

    std::wstring result =
        noteNames[note];

    result +=
        std::to_wstring(octave);

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

void CreateKeys()
{
    keys.clear();

    // Piano range:
    //
    // A0 = MIDI 21
    // C8 = MIDI 108
    //
    // 108 - 21 + 1 = 88 keys

    for (int midi = 21;
         midi <= 108;
         ++midi)
    {
        PianoKey key;

        key.midiNote =
            midi;

        key.noteName =
            MidiToNoteName(
                midi
            );

        key.frequency =
            MidiToFrequency(
                midi
            );

        key.black =
            IsBlackKey(
                midi
            );

        key.pressed = false;

        keys.push_back(
            key
        );
    }
}

// ============================================================
// GET KEY AT MOUSE POSITION
// ============================================================

PianoKey* FindKeyAt(
    int x,
    int y)
{
    // Check black keys first because
    // they sit on top of white keys.

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

    // Then check white keys.

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
// WAVEFORM HELPERS
// ============================================================

double Sine(double phase)
{
    return std::sin(
        2.0 * PI * phase
    );
}

// ============================================================
// GENERATE INSTRUMENT SAMPLE
// ============================================================

double GenerateInstrumentSample(
    Instrument instrument,
    double frequency,
    double t)
{
    double phase =
        frequency * t;

    double value = 0.0;

    switch (instrument)
    {
        // ----------------------------------------------------
        // PIANO
        // ----------------------------------------------------

        case Instrument::PIANO:
        {
            value =
                0.85 * Sine(phase) +
                0.30 * Sine(phase * 2.0) +
                0.18 * Sine(phase * 3.0) +
                0.10 * Sine(phase * 4.0);

            break;
        }

        // ----------------------------------------------------
        // FLUTE
        // ----------------------------------------------------

        case Instrument::FLUTE:
        {
            value =
                0.92 * Sine(phase) +
                0.08 * Sine(phase * 2.0);

            break;
        }

        // ----------------------------------------------------
        // ORGAN
        // ----------------------------------------------------

        case Instrument::ORGAN:
        {
            value =
                0.55 * Sine(phase) +
                0.35 * Sine(phase * 2.0) +
                0.25 * Sine(phase * 3.0) +
                0.18 * Sine(phase * 4.0) +
                0.10 * Sine(phase * 6.0);

            break;
        }

        // ----------------------------------------------------
        // BELL
        // ----------------------------------------------------

        case Instrument::BELL:
        {
            value =
                0.75 * Sine(phase) +
                0.55 * Sine(phase * 2.01) +
                0.35 * Sine(phase * 3.02) +
                0.20 * Sine(phase * 4.17);

            // Bell naturally fades faster.
            value *=
                std::exp(-3.5 * t);

            break;
        }

        // ----------------------------------------------------
        // STRINGS
        // ----------------------------------------------------

        case Instrument::STRINGS:
        {
            double vibrato =
                0.003 *
                std::sin(
                    2.0 * PI * 5.0 * t
                );

            double stringPhase =
                frequency *
                (1.0 + vibrato) *
                t;

            value =
                0.65 * Sine(stringPhase) +
                0.30 * Sine(stringPhase * 2.0) +
                0.15 * Sine(stringPhase * 3.0);

            break;
        }

        // ----------------------------------------------------
        // SYNTH
        // ----------------------------------------------------

        case Instrument::SYNTH:
        {
            // Saw-like waveform made from harmonics.

            for (int harmonic = 1;
                 harmonic <= 8;
                 ++harmonic)
            {
                value +=
                    (1.0 / harmonic) *
                    Sine(
                        phase *
                        harmonic
                    );
            }

            value *= 0.55;

            break;
        }
    }

    return value;
}

// ============================================================
// PLAY NOTE SOUND
// ============================================================

void PlayNoteSound(
    double frequency,
    Instrument instrument)
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

            for (int i = 0;
                 i < sampleCount;
                 ++i)
            {
                double t =
                    static_cast<double>(i) /
                    SAMPLE_RATE;

                double envelope = 1.0;

                // Quick attack.
                if (t < 0.015)
                {
                    envelope =
                        t / 0.015;
                }

                // Release.
                double releaseStart =
                    NOTE_LENGTH * 0.65;

                if (t > releaseStart)
                {
                    double remaining =
                        NOTE_LENGTH - t;

                    double releaseLength =
                        NOTE_LENGTH -
                        releaseStart;

                    envelope *=
                        remaining /
                        releaseLength;
                }

                double value =
                    GenerateInstrumentSample(
                        instrument,
                        frequency,
                        t
                    );

                value *=
                    envelope;

                // Requested 5X boost.
                value *=
                    VOLUME_BOOST;

                // Soft clipping.
                value =
                    std::tanh(value);

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
            {
                return;
            }

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

            if (result ==
                MMSYSERR_NOERROR)
            {
                result =
                    waveOutWrite(
                        waveOut,
                        &header,
                        sizeof(header)
                    );

                if (result ==
                    MMSYSERR_NOERROR)
                {
                    while (
                        !(header.dwFlags &
                          WHDR_DONE))
                    {
                        Sleep(5);
                    }
                }

                waveOutUnprepareHeader(
                    waveOut,
                    &header,
                    sizeof(header)
                );
            }

            waveOutClose(
                waveOut
            );
        }
    ).detach();
}

// ============================================================
// PRINT NOTE
// ============================================================

void PrintNote(
    const PianoKey& key)
{
    if (outputEditor == nullptr)
        return;

    std::wstring instrument =
        instrumentNames[
            static_cast<int>(
                currentInstrument
            )
        ];

    // IMPORTANT:
    //
    // The format is:
    //
    // INSTRUMENT NOTE POSITION DURATION
    //
    // Example:
    //
    // FLUTE D#2 0 1
    // FLUTE F2 1 1
    // FLUTE A#2 2 1

    std::wstringstream line;

    line
        << instrument
        << L" "
        << key.noteName
        << L" "
        << notePosition
        << L" "
        << L"1\r\n";

    int length =
        GetWindowTextLengthW(
            outputEditor
        );

    std::wstring existing;

    if (length > 0)
    {
        existing.resize(
            length
        );

        GetWindowTextW(
            outputEditor,
            &existing[0],
            length + 1
        );
    }

    existing +=
        line.str();

    SetWindowTextW(
        outputEditor,
        existing.c_str()
    );

    // Move to the newest note.
    SendMessageW(
        outputEditor,
        EM_SETSEL,
        static_cast<WPARAM>(
            existing.length()
        ),
        static_cast<LPARAM>(
            existing.length()
        )
    );

    // Advance the position for
    // the NEXT note.
    ++notePosition;
}

// ============================================================
// RESIZE PIANO
// ============================================================

void ResizePiano()
{
    if (mainWindow == nullptr)
        return;

    RECT rect{};

    GetClientRect(
        mainWindow,
        &rect
    );

    int width =
        rect.right;

    int height =
        rect.bottom;

    // Piano starts below the controls.
    int pianoTop = 165;

    int pianoHeight =
        height - pianoTop - 15;

    if (pianoHeight < 100)
        pianoHeight = 100;

    int rowHeight =
        pianoHeight / 2;

    // --------------------------------------------------------
    // Two rows of 44 keys.
    // --------------------------------------------------------

    for (size_t i = 0;
         i < keys.size();
         ++i)
    {
        PianoKey& key =
            keys[i];

        int row =
            static_cast<int>(
                i / 44
            );

        int rowStart =
            row * 44;

        int rowEnd =
            std::min(
                rowStart + 44,
                static_cast<int>(
                    keys.size()
                )
            );

        // Count white keys in this row.
        int whiteCount = 0;

        for (int j = rowStart;
             j < rowEnd;
             ++j)
        {
            if (!keys[j].black)
                ++whiteCount;
        }

        if (whiteCount <= 0)
            continue;

        int whiteWidth =
            std::max(
                1,
                width / whiteCount
            );

        // Determine the white-key index
        // of this key within its row.
        int whiteIndex = 0;

        for (int j = rowStart;
             j < static_cast<int>(i);
             ++j)
        {
            if (!keys[j].black)
                ++whiteIndex;
        }

        int top =
            pianoTop +
            row * rowHeight;

        int bottom =
            top + rowHeight;

        if (!key.black)
        {
            int left =
                whiteIndex *
                whiteWidth;

            int right =
                (whiteIndex + 1) *
                whiteWidth;

            // Make final key reach the edge.
            if (whiteIndex ==
                whiteCount - 1)
            {
                right = width;
            }

            key.rect =
            {
                left,
                top,
                right,
                bottom
            };
        }
        else
        {
            // Black keys are positioned between
            // their neighboring white keys.

            int note =
                key.midiNote % 12;

            int previousWhiteIndex =
                whiteIndex - 1;

            // For black keys, whiteIndex is
            // the number of whites before it.
            //
            // Place it centered at the boundary.

            double center =
                static_cast<double>(
                    whiteIndex *
                    whiteWidth
                );

            int blackWidth =
                std::max(
                    8,
                    whiteWidth * 58 / 100
                );

            int left =
                static_cast<int>(
                    center -
                    blackWidth / 2.0
                );

            int right =
                left +
                blackWidth;

            int blackHeight =
                rowHeight * 58 / 100;

            // Keep inside row.
            if (left < 0)
                left = 0;

            if (right > width)
                right = width;

            key.rect =
            {
                left,
                top,
                right,
                top + blackHeight
            };

            (void)previousWhiteIndex;
            (void)note;
        }
    }

    InvalidateRect(
        mainWindow,
        nullptr,
        FALSE
    );
}

// ============================================================
// DRAW PIANO
// ============================================================

void DrawPiano(
    HDC hdc)
{
    // White keys first.
    for (PianoKey& key : keys)
    {
        if (key.black)
            continue;

        HBRUSH brush =
            CreateSolidBrush(
                key.pressed
                    ? RGB(180, 220, 255)
                    : RGB(250, 250, 250)
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

        // Note name at bottom.
        SetBkMode(
            hdc,
            TRANSPARENT
        );

        SetTextColor(
            hdc,
            RGB(30, 30, 30)
        );

        RECT textRect =
            key.rect;

        textRect.top +=
            (textRect.bottom -
             textRect.top) - 25;

        DrawTextW(
            hdc,
            key.noteName.c_str(),
            -1,
            &textRect,
            DT_CENTER |
            DT_SINGLELINE |
            DT_VCENTER
        );
    }

    // Black keys second so they appear
    // on top of white keys.
    for (PianoKey& key : keys)
    {
        if (!key.black)
            continue;

        HBRUSH brush =
            CreateSolidBrush(
                key.pressed
                    ? RGB(60, 100, 150)
                    : RGB(25, 25, 25)
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
}

// ============================================================
// UPDATE INSTRUMENT BUTTON
// ============================================================

void UpdateInstrumentButton()
{
    if (instrumentButton == nullptr)
        return;

    std::wstring text =
        L"Instrument: ";

    text +=
        instrumentNames[
            static_cast<int>(
                currentInstrument
            )
        ];

    SetWindowTextW(
        instrumentButton,
        text.c_str()
    );
}

// ============================================================
// CLEAR OUTPUT
// ============================================================

void ClearOutput()
{
    notePosition = 0;

    if (outputEditor != nullptr)
    {
        SetWindowTextW(
            outputEditor,
            L""
        );
    }

    for (PianoKey& key : keys)
    {
        key.pressed = false;
    }

    InvalidateRect(
        mainWindow,
        nullptr,
        FALSE
    );
}

// ============================================================
// COPY OUTPUT
// ============================================================

void CopyOutput()
{
    if (outputEditor == nullptr)
        return;

    int length =
        GetWindowTextLengthW(
            outputEditor
        );

    if (length <= 0)
        return;

    std::wstring text;

    text.resize(
        length
    );

    GetWindowTextW(
        outputEditor,
        &text[0],
        length + 1
    );

    if (!OpenClipboard(
            mainWindow))
    {
        return;
    }

    EmptyClipboard();

    SIZE_T bytes =
        (text.size() + 1) *
        sizeof(wchar_t);

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            bytes
        );

    if (memory != nullptr)
    {
        void* data =
            GlobalLock(
                memory
            );

        if (data != nullptr)
        {
            memcpy(
                data,
                text.c_str(),
                bytes
            );

            GlobalUnlock(
                memory
            );

            SetClipboardData(
                CF_UNICODETEXT,
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
                CreateWindowW(
                    L"BUTTON",
                    L"Instrument: FLUTE",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_PUSHBUTTON,
                    15,
                    75,
                    220,
                    35,
                    window,
                    reinterpret_cast<HMENU>(
                        1001
                    ),
                    GetModuleHandleW(
                        nullptr
                    ),
                    nullptr
                );

            clearButton =
                CreateWindowW(
                    L"BUTTON",
                    L"Clear",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_PUSHBUTTON,
                    245,
                    75,
                    100,
                    35,
                    window,
                    reinterpret_cast<HMENU>(
                        1002
                    ),
                    GetModuleHandleW(
                        nullptr
                    ),
                    nullptr
                );

            copyButton =
                CreateWindowW(
                    L"BUTTON",
                    L"Copy",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_PUSHBUTTON,
                    355,
                    75,
                    100,
                    35,
                    window,
                    reinterpret_cast<HMENU>(
                        1003
                    ),
                    GetModuleHandleW(
                        nullptr
                    ),
                    nullptr
                );

            outputEditor =
                CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    L"",
                    WS_CHILD |
                    WS_VISIBLE |
                    ES_MULTILINE |
                    ES_AUTOVSCROLL |
                    ES_AUTOHSCROLL |
                    WS_VSCROLL |
                    WS_HSCROLL,
                    470,
                    70,
                    400,
                    75,
                    window,
                    reinterpret_cast<HMENU>(
                        1004
                    ),
                    GetModuleHandleW(
                        nullptr
                    ),
                    nullptr
                );

            SendMessageW(
                outputEditor,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    GetStockObject(
                        DEFAULT_GUI_FONT
                    )
                ),
                TRUE
            );

            return 0;
        }

        // ====================================================
        // BUTTON
        // ====================================================

        case WM_COMMAND:
        {
            int controlID =
                LOWORD(wParam);

            if (controlID == 1001)
            {
                int instrument =
                    static_cast<int>(
                        currentInstrument
                    );

                ++instrument;

                if (instrument >=
                    static_cast<int>(
                        instrumentNames.size()
                    ))
                {
                    instrument = 0;
                }

                currentInstrument =
                    static_cast<Instrument>(
                        instrument
                    );

                UpdateInstrumentButton();

                return 0;
            }

            if (controlID == 1002)
            {
                ClearOutput();

                return 0;
            }

            if (controlID == 1003)
            {
                CopyOutput();

                return 0;
            }

            return 0;
        }

        // ====================================================
        // LEFT MOUSE
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
                for (PianoKey& current :
                     keys)
                {
                    current.pressed =
                        false;
                }

                key->pressed =
                    true;

                // Play the note.
                PlayNoteSound(
                    key->frequency,
                    currentInstrument
                );

                // Print the note.
                PrintNote(
                    *key
                );

                InvalidateRect(
                    window,
                    nullptr,
                    FALSE
                );
            }

            SetCapture(
                window
            );

            return 0;
        }

        // ====================================================
        // MOUSE MOVE
        // ====================================================

        case WM_MOUSEMOVE:
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

            bool leftButtonDown =
                (wParam & MK_LBUTTON) != 0;

            if (leftButtonDown)
            {
                PianoKey* key =
                    FindKeyAt(
                        x,
                        y
                    );

                if (key != nullptr)
                {
                    // Only trigger when moving
                    // onto a different key.

                    if (!key->pressed)
                    {
                        for (PianoKey& current :
                             keys)
                        {
                            current.pressed =
                                false;
                        }

                        key->pressed =
                            true;

                        PlayNoteSound(
                            key->frequency,
                            currentInstrument
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
                }
            }

            return 0;
        }

        // ====================================================
        // LEFT MOUSE UP
        // ====================================================

        case WM_LBUTTONUP:
        {
            ReleaseCapture();

            for (PianoKey& key :
                 keys)
            {
                key.pressed =
                    false;
            }

            InvalidateRect(
                window,
                nullptr,
                FALSE
            );

            return 0;
        }

        // ====================================================
        // RESIZE
        // ====================================================

        case WM_SIZE:
        {
            ResizePiano();

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

            RECT client{};

            GetClientRect(
                window,
                &client
            );

            HBRUSH background =
                CreateSolidBrush(
                    RGB(35, 35, 35)
                );

            FillRect(
                hdc,
                &client,
                background
            );

            DeleteObject(
                background
            );

            // Title.
            SetBkMode(
                hdc,
                TRANSPARENT
            );

            SetTextColor(
                hdc,
                RGB(255, 255, 255)
            );

            RECT titleRect =
            {
                15,
                15,
                client.right - 15,
                55
            };

            DrawTextW(
                hdc,
                L"88-Key Music Code Piano",
                -1,
                &titleRect,
                DT_LEFT |
                DT_SINGLELINE |
                DT_VCENTER
            );

            // Small information text.
            RECT infoRect =
            {
                15,
                50,
                client.right - 15,
                70
            };

            std::wstring info =
                L"Click or drag across the keys to play and print notes";

            SetTextColor(
                hdc,
                RGB(190, 190, 190)
            );

            DrawTextW(
                hdc,
                info.c_str(),
                -1,
                &infoRect,
                DT_LEFT |
                DT_SINGLELINE |
                DT_VCENTER
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
        // CLOSE
        // ====================================================

        case WM_DESTROY:
        {
            PostQuitMessage(
                0
            );

            return 0;
        }
    }

    return DefWindowProcW(
        window,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// MAIN
// ============================================================

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int nCmdShow)
{
    const wchar_t CLASS_NAME[] =
        L"MusicCodePianoWindow";

    WNDCLASSW wc{};

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW
        );

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1
        );

    if (!RegisterClassW(
            &wc))
    {
        MessageBoxW(
            nullptr,
            L"Could not register the window class.",
            L"Error",
            MB_ICONERROR
        );

        return 1;
    }

    CreateKeys();

    mainWindow =
        CreateWindowExW(
            0,
            CLASS_NAME,
            L"88-Key Music Code Piano",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1200,
            700,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );

    if (mainWindow == nullptr)
    {
        MessageBoxW(
            nullptr,
            L"Could not create the window.",
            L"Error",
            MB_ICONERROR
        );

        return 1;
    }

    ShowWindow(
        mainWindow,
        nCmdShow
    );

    UpdateWindow(
        mainWindow
    );

    ResizePiano();

    MSG msg{};

    while (
        GetMessageW(
            &msg,
            nullptr,
            0,
            0
        ) > 0)
    {
        TranslateMessage(
            &msg
        );

        DispatchMessageW(
            &msg
        );
    }

    return static_cast<int>(
        msg.wParam
    );
}
