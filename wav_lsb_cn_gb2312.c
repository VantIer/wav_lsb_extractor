#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// WAV文件头结构体
#pragma pack(push, 1) // 确保结构体按1字节对齐
typedef struct {
    char     chunk_id[4];      // "RIFF"
    uint32_t chunk_size;       // 文件总大小-8
    char     format[4];        // "WAVE"
    char     subchunk1_id[4];  // "fmt "
    uint32_t subchunk1_size;   // fmt块大小(16)
    uint16_t audio_format;     // 音频格式(1=PCM)
    uint16_t num_channels;     // 声道数
    uint32_t sample_rate;      // 采样率
    uint32_t byte_rate;        // 每秒字节数
    uint16_t block_align;      // 每个样本的字节数
    uint16_t bits_per_sample;  // 每个样本的位数
} WavHeader;
#pragma pack(pop)

char     subchunk2_id[4] = {0};  // "data"
uint32_t subchunk2_size = 0;   // 数据块大小

uint8_t select_to_extract[16*16] = {0};  // 最多支持16bits位宽度，16轨道的bit提取

void print_wav_info(WavHeader *header) {
    printf("\n");
    printf("WAV文件信息:\n");
    printf("  文件格式: \t%.4s\n", header->format);
    printf("  音频格式: \t%s\n", header->audio_format == 1 ? "PCM" : "其他");
    printf("  声道数: \t%u\n", header->num_channels);
    printf("  采样率: \t%u Hz\n", header->sample_rate);
    printf("  比特率: \t%u bps\n", header->byte_rate * 8);
    printf("  块对齐: \t%u bytes\n", header->block_align);
    printf("  样本位数: \t%u bits\n", header->bits_per_sample);
    printf("  数据大小: \t%u bytes\n", subchunk2_size);

    // 计算持续时间
    double duration = (double)subchunk2_size / header->byte_rate;
    printf("  持续时间: \t%.2f 秒\n", duration);
}

void print_to_select(WavHeader *header, int input_check) {
    int line = 0;
    int colum = 0;
    int count = header->num_channels * header->bits_per_sample - 1;

    // 输出轨道信息和bit位对应的值，用于选择
    printf("\n");
    for (line = 0; line < header->num_channels; line++) {
        // 输出轨道信息
        printf("轨道 %d (高位 <-> 低位):\n", line);
        for (colum = 0; colum < header->bits_per_sample; colum++) {
            // 输出轨道内的位索引
            // 选中的位加个提示
            if (input_check == 1 && select_to_extract[count] == 1) {
                printf(" [");
            } else {
                printf(" ");
            }
            printf("%d", count);
            if (input_check == 1 && select_to_extract[count] == 1) {
                printf("] ");
            } else {
                printf(" ");
            }
            if (count < 10) {
                printf(" ");
            }
            count--;
        }
        printf("\n");
    }
}

// 位宽为8时的读取和处理
void extract_8(FILE *file, WavHeader *header, int mode, const char *output_filename) {
    // 存储读入的数据
    uint8_t data_to_read = 0;
    size_t bytes_read = 0;
    // 存储转换好的数据
    uint8_t char_to_out = 0;
    int bit_count = 0;  // 记录已经累积的位数

    int total_bits = header->num_channels * header->bits_per_sample;
    int current_bit_index = total_bits - 1;  // 从最高位开始

    FILE *file_output = fopen(output_filename, "w");
    if (!file_output) {
        printf("无法打开输出文件: %s\n", output_filename);
        return;
    }

    printf("开始提取数据...\n");

    while (1) {
        // 读取一个样本（包含所有声道）
        bytes_read = fread(&data_to_read, 1, 1, file);
        if (bytes_read != 1) {
            break;
        }

        // 处理一个样本中的所有位
        for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
            // 检查当前位是否被选中
            if (select_to_extract[current_bit_index]) {
                // 提取当前位并添加到输出字符中
                uint8_t current_bit = (data_to_read >> bit_pos) & 0x01;
                char_to_out = (char_to_out << 1) | current_bit;
                bit_count++;

                // 当累积到8位时输出
                if (bit_count == 8) {
                    if (mode == 0) {
                        // 字符模式：直接输出字符
                        fwrite(&char_to_out, 1, 1, file_output);
                    } else {
                        // 二进制模式：输出二进制字符串
                        char binary_str[9];
                        for (int i = 7; i >= 0; i--) {
                            binary_str[7-i] = ((char_to_out >> i) & 0x01) ? '1' : '0';
                        }
                        binary_str[8] = '\0';
                        fprintf(file_output, "%s", binary_str);
                    }
                    char_to_out = 0;
                    bit_count = 0;
                }
            }

            // 移动到下一个位索引
            current_bit_index--;
            if (current_bit_index < 0) {
                current_bit_index = total_bits - 1;  // 重置到最高位
            }
        }
    }

    // 处理剩余未满8位的部分
    if (bit_count > 0) {
        // 左对齐剩余位
        char_to_out = char_to_out << (8 - bit_count);

        if (mode == 0) {
            fwrite(&char_to_out, 1, 1, file_output);
        } else {
            char binary_str[9];
            for (int i = 7; i >= 0; i--) {
                binary_str[7-i] = ((char_to_out >> i) & 0x01) ? '1' : '0';
            }
            binary_str[8] = '\0';
            fprintf(file_output, "%s", binary_str);
        }
        printf("警告: 有 %d 位未满8位，已用0填充\n", 8 - bit_count);
    }

    fclose(file_output);
    printf("数据提取完成，结果已保存到 %s\n", output_filename);
}

// 位宽为16时的读取和处理
void extract_16(FILE *file, WavHeader *header, int mode, const char *output_filename) {
    // 存储读入的数据
    uint16_t data_to_read = 0;
    size_t bytes_read = 0;
    // 存储转换好的数据
    uint8_t char_to_out = 0;
    int bit_count = 0;  // 记录已经累积的位数

    int total_bits = header->num_channels * header->bits_per_sample;
    int current_bit_index = total_bits - 1;  // 从最高位开始

    FILE *file_output = fopen(output_filename, "w");
    if (!file_output) {
        printf("无法打开输出文件: %s\n", output_filename);
        return;
    }

    printf("开始提取数据...\n");

    while (1) {
        // 读取一个样本（包含所有声道）
        bytes_read = fread(&data_to_read, 1, 2, file);
        if (bytes_read != 2) {
            break;
        }

        // 处理一个样本中的所有位
        for (int bit_pos = 15; bit_pos >= 0; bit_pos--) {
            // 检查当前位是否被选中
            if (select_to_extract[current_bit_index]) {
                // 提取当前位并添加到输出字符中
                uint8_t current_bit = (data_to_read >> bit_pos) & 0x01;
                char_to_out = (char_to_out << 1) | current_bit;
                bit_count++;

                // 当累积到8位时输出
                if (bit_count == 8) {
                    if (mode == 0) {
                        // 字符模式：直接输出字符
                        fwrite(&char_to_out, 1, 1, file_output);
                    } else {
                        // 二进制模式：输出二进制字符串
                        char binary_str[9];
                        for (int i = 7; i >= 0; i--) {
                            binary_str[7-i] = ((char_to_out >> i) & 0x01) ? '1' : '0';
                        }
                        binary_str[8] = '\0';
                        fprintf(file_output, "%s", binary_str);
                    }
                    char_to_out = 0;
                    bit_count = 0;
                }
            }

            // 移动到下一个位索引
            current_bit_index--;
            if (current_bit_index < 0) {
                current_bit_index = total_bits - 1;  // 重置到最高位
            }
        }
    }

    // 处理剩余未满8位的部分
    if (bit_count > 0) {
        // 左对齐剩余位
        char_to_out = char_to_out << (8 - bit_count);

        if (mode == 0) {
            fwrite(&char_to_out, 1, 1, file_output);
        } else {
            char binary_str[9];
            for (int i = 7; i >= 0; i--) {
                binary_str[7-i] = ((char_to_out >> i) & 0x01) ? '1' : '0';
            }
            binary_str[8] = '\0';
            fprintf(file_output, "%s", binary_str);
        }
        printf("警告: 有 %d 位未满8位，已用0填充\n", 8 - bit_count);
    }

    fclose(file_output);
    printf("数据提取完成，结果已保存到 %s\n", output_filename);
}

int main(int argc, char *argv[]) {

    printf("\n-= WAV音频bit提取工具 v0.1 By Vantler =-\n");

    if (argc != 3) {
        printf("用法: %s <wav文件> <输出文件>\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    FILE *file = fopen(input_filename, "rb");
    if (!file) {
        printf("无法打开WAV文件: %s\n", input_filename);
        return 1;
    }

    WavHeader header;

    // 读取WAV文件头
    size_t bytes_read = fread(&header, 1, sizeof(WavHeader), file);
    if (bytes_read != sizeof(WavHeader)) {
        printf("读取文件头失败\n");
        fclose(file);
        return 1;
    }

    // 验证文件格式
    if (memcmp(header.chunk_id, "RIFF", 4) != 0 ||
        memcmp(header.format, "WAVE", 4) != 0) {
        printf("不是有效的WAV文件\n");
        fclose(file);
        return 1;
    }

    // 如果fmt块大小大于16，需要跳过额外字节
    if (header.subchunk1_size > 16) {
        fseek(file, header.subchunk1_size - 16, SEEK_CUR);
    }

    // 查找data块（有些WAV文件可能有其他块在fmt和data之间）
    while (1) {
        bytes_read = fread(subchunk2_id, 1, 4, file);
        bytes_read += fread(&subchunk2_size, 1, 4, file);

        if (bytes_read != 8) {
            printf("找不到data块\n");
            fclose(file);
            return 1;
        }

        if (memcmp(subchunk2_id, "data", 4) == 0) {
            break;
        } else {
            // 跳过这个块
            fseek(file, subchunk2_size, SEEK_CUR);
        }
    }

    print_wav_info(&header);
    if (header.audio_format != 1) {
        printf("不受支持的音频数据格式 %d (非PCM)，无法提取数据\n", header.audio_format);
        return 0;
    }

    // 根据wav的声道数和位长度显示提示，获取待提取目标
    print_to_select(&header, 0);
    int input = 0;
    printf("\n请输入要提取的位索引 (范围: 0 - %d)\n", header.num_channels * header.bits_per_sample - 1);
    printf("输入范围外的任意数字结束选择\n");
    while (1) {
        printf("索引: ");
        if (scanf("%d", &input) != 1) {
            // 清除输入缓冲区
            while (getchar() != '\n');
            printf("输入无效，请输入数字\n");
            continue;
        }
        // 检查是否结束输入
        if (input < 0 || input > header.num_channels * header.bits_per_sample - 1) {
            printf("索引输入结束\n");
            break;
        }
        // 设置对应的选择位
        select_to_extract[input] ^= 1;
        print_to_select(&header, 1);
        printf("\n");
    }

    int mode = 0;
    printf("\n请选择数据输出模式，0为字符模式，1为二进制模式\n");
    while (1) {
        printf("模式: ");
        if (scanf("%d", &mode) != 1) {
            // 清除输入缓冲区
            while (getchar() != '\n');
            printf("输入无效，请输入数字\n");
            continue;
        }
        // 检查是否结束输入
        if (mode == 0 || mode == 1) {
            printf("已选择%s模式\n", mode ? "二进制" : "字符");
            break;
        }
        // 设置对应的选择位
        printf("输入错误，请输入0或1\n");
    }

    // 根据不同的长度解压数据
    switch(header.bits_per_sample) {
    case 8:
        extract_8(file, &header, mode, output_filename);
        break;
    case 16:
        extract_16(file, &header, mode, output_filename);
        break;
    default:
        printf("不受支持的位宽度: %d bits\n", header.bits_per_sample);
        break;
    }

    fclose(file);
    return 0;
}
