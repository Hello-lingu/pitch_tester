#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <portaudio.h>
#include <kissfft/kiss_fft.h>
#include <kissfft/kiss_fftnd.h>
#include <LCUI.h>
#include <LCUI/main.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/widget/textview.h>
#include <LCUI/gui/builder.h>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 1024
#define FFT_SIZE 1024

static volatile int running = 1;
static LCUI_Widget root, note_display, freq_display;

// 汉宁窗生成
void generate_hanning_window(float *window, int size) {
    for (int i = 0; i < size; i++) {
        window[i] = 0.5f * (1 - cosf(2 * M_PI * i / (size - 1)));
    }
}

// 将频率转换为音高
void frequency_to_note(float freq, char *note, int *octave) {
    const char *notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (freq < 27.5f) freq = 27.5f; // 最低A0的频率

    float midi_note = 12.0f * log2f(freq / 440.0f) + 69.0f;
    int note_num = (int)(midi_note + 0.5f); // 四舍五入

    *octave = (note_num / 12) - 1;
    sprintf(note, "%s", notes[note_num % 12]);
}

void quit() {
    LCUI_Quit();
    running = 0;
}

int main() {
    PaError err = 0;
    PaStream *stream = NULL;
    int16_t buffer[FRAMES_PER_BUFFER];
    float window[FFT_SIZE];
    float samples[FFT_SIZE];
    LCUI_Widget xml = NULL;

    LCUI_Init();

    xml = LCUIBuilder_LoadFile("ui.xml");
    if (!xml) {
        fprintf(stderr, "count not load xml file\n");
        return 1;
    }

    root = LCUIWidget_GetRoot();
    Widget_Unwrap(xml);

    // 获取界面组件引用
    freq_display = LCUIWidget_GetById("freq-display");
    note_display = LCUIWidget_GetById("note-display");
    LCUI_Widget btn_exit = LCUIWidget_GetById("btn-exit");

    Widget_BindEvent(btn_exit, "click", quit, NULL, NULL);

    Widget_Append(root, note_display);
    Widget_Append(root, freq_display);
    Widget_Append(root, btn_exit);

    // 初始化PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio初始化失败: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    // 打开默认输入流
    err = Pa_OpenDefaultStream(&stream, 1, 0, paInt16, SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL);
    if (err != paNoError) {
        fprintf(stderr, "无法打开音频流: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return 1;
    }

    // 启动音频流
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "无法启动音频流: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    // 生成汉宁窗
    generate_hanning_window(window, FFT_SIZE);

    printf("正在监听麦克风... 按Ctrl+C退出\n");

    while (running) {
        // 读取音频数据
        err = Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
        if (err != paNoError) {
            fprintf(stderr, "音频读取错误: %s\n", Pa_GetErrorText(err));
            continue;
        }

        // 转换为浮点数并应用窗函数
        for (int i = 0; i < FFT_SIZE; i++) {
            samples[i] = (float)buffer[i] / 32768.0f * window[i];
        }

        // 准备FFT
        kiss_fft_cfg cfg = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);
        kiss_fft_cpx fft_in[FFT_SIZE], fft_out[FFT_SIZE];

        for (int i = 0; i < FFT_SIZE; i++) {
            fft_in[i].r = samples[i];
            fft_in[i].i = 0;
        }

        // 执行FFT
        kiss_fft(cfg, fft_in, fft_out);
        kiss_fft_free(cfg);

        // 寻找最大幅值的频率
        float max_mag = 0;
        int max_idx = 0;
        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float mag = sqrtf(fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i);
            if (mag > max_mag) {
                max_mag = mag;
                max_idx = i;
            }
        }

        // 计算对应频率（简单峰值检测）
        float frequency = (float)max_idx * SAMPLE_RATE / FFT_SIZE;

        // 转换为音高
        char note[3];
        int octave;
        frequency_to_note(frequency, note, &octave);

        // printf("频率: %6.1f Hz | 音高: %s%d\n", frequency, note, octave);

        char freq_text[64], note_text[64];
        snprintf(freq_text, sizeof(freq_text), "%6.1f Hz", frequency);
        snprintf(note_text, sizeof(note_text), "%s%d", note, octave);

        Widget_SetText(freq_display, freq_text);
        Widget_SetText(note_display, note_text);

        LCUI_ProcessEvents();
        LCUI_RunFrame();
    }

    // 清理资源
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    LCUI_Destroy();

    return 0;
}
