#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// WAV�ļ�ͷ�ṹ��
#pragma pack(push, 1) // ȷ���ṹ�尴1�ֽڶ���
typedef struct {
    char     chunk_id[4];      // "RIFF"
    uint32_t chunk_size;       // �ļ��ܴ�С-8
    char     format[4];        // "WAVE"
    char     subchunk1_id[4];  // "fmt "
    uint32_t subchunk1_size;   // fmt���С(16)
    uint16_t audio_format;     // ��Ƶ��ʽ(1=PCM)
    uint16_t num_channels;     // ������
    uint32_t sample_rate;      // ������
    uint32_t byte_rate;        // ÿ���ֽ���
    uint16_t block_align;      // ÿ���������ֽ���
    uint16_t bits_per_sample;  // ÿ��������λ��
} WavHeader;
#pragma pack(pop)

char     subchunk2_id[4] = {0};  // "data"
uint32_t subchunk2_size = 0;   // ���ݿ��С

uint8_t select_to_extract[16*16] = {0};  // ���֧��16bitsλ��ȣ�16�����bit��ȡ

void print_wav_info(WavHeader *header) {
    printf("\n");
    printf("WAV�ļ���Ϣ:\n");
    printf("  �ļ���ʽ: \t%.4s\n", header->format);
    printf("  ��Ƶ��ʽ: \t%s\n", header->audio_format == 1 ? "PCM" : "����");
    printf("  ������: \t%u\n", header->num_channels);
    printf("  ������: \t%u Hz\n", header->sample_rate);
    printf("  ������: \t%u bps\n", header->byte_rate * 8);
    printf("  �����: \t%u bytes\n", header->block_align);
    printf("  ����λ��: \t%u bits\n", header->bits_per_sample);
    printf("  ���ݴ�С: \t%u bytes\n", subchunk2_size);

    // �������ʱ��
    double duration = (double)subchunk2_size / header->byte_rate;
    printf("  ����ʱ��: \t%.2f ��\n", duration);
}

void print_to_select(WavHeader *header, int input_check) {
    int line = 0;
    int colum = 0;
    int count = header->num_channels * header->bits_per_sample - 1;

    // ��������Ϣ��bitλ��Ӧ��ֵ������ѡ��
    printf("\n");
    for (line = 0; line < header->num_channels; line++) {
        // ��������Ϣ
        printf("��� %d (��λ <-> ��λ):\n", line);
        for (colum = 0; colum < header->bits_per_sample; colum++) {
            // �������ڵ�λ����
            // ѡ�е�λ�Ӹ���ʾ
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

// λ��Ϊ8ʱ�Ķ�ȡ�ʹ���
void extract_8(FILE *file, WavHeader *header, int mode, const char *output_filename) {
    // �洢���������
    uint8_t data_to_read = 0;
    size_t bytes_read = 0;
    // �洢ת���õ�����
    uint8_t char_to_out = 0;
    int bit_count = 0;  // ��¼�Ѿ��ۻ���λ��

    int total_bits = header->num_channels * header->bits_per_sample;
    int current_bit_index = total_bits - 1;  // �����λ��ʼ

    FILE *file_output = fopen(output_filename, "w");
    if (!file_output) {
        printf("�޷�������ļ�: %s\n", output_filename);
        return;
    }

    printf("��ʼ��ȡ����...\n");

    while (1) {
        // ��ȡһ����������������������
        bytes_read = fread(&data_to_read, 1, 1, file);
        if (bytes_read != 1) {
            break;
        }

        // ����һ�������е�����λ
        for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
            // ��鵱ǰλ�Ƿ�ѡ��
            if (select_to_extract[current_bit_index]) {
                // ��ȡ��ǰλ����ӵ�����ַ���
                uint8_t current_bit = (data_to_read >> bit_pos) & 0x01;
                char_to_out = (char_to_out << 1) | current_bit;
                bit_count++;

                // ���ۻ���8λʱ���
                if (bit_count == 8) {
                    if (mode == 0) {
                        // �ַ�ģʽ��ֱ������ַ�
                        fwrite(&char_to_out, 1, 1, file_output);
                    } else {
                        // ������ģʽ������������ַ���
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

            // �ƶ�����һ��λ����
            current_bit_index--;
            if (current_bit_index < 0) {
                current_bit_index = total_bits - 1;  // ���õ����λ
            }
        }
    }

    // ����ʣ��δ��8λ�Ĳ���
    if (bit_count > 0) {
        // �����ʣ��λ
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
        printf("����: �� %d λδ��8λ������0���\n", 8 - bit_count);
    }

    fclose(file_output);
    printf("������ȡ��ɣ�����ѱ��浽 %s\n", output_filename);
}

// λ��Ϊ16ʱ�Ķ�ȡ�ʹ���
void extract_16(FILE *file, WavHeader *header, int mode, const char *output_filename) {
    // �洢���������
    uint16_t data_to_read = 0;
    size_t bytes_read = 0;
    // �洢ת���õ�����
    uint8_t char_to_out = 0;
    int bit_count = 0;  // ��¼�Ѿ��ۻ���λ��

    int total_bits = header->num_channels * header->bits_per_sample;
    int current_bit_index = total_bits - 1;  // �����λ��ʼ

    FILE *file_output = fopen(output_filename, "w");
    if (!file_output) {
        printf("�޷�������ļ�: %s\n", output_filename);
        return;
    }

    printf("��ʼ��ȡ����...\n");

    while (1) {
        // ��ȡһ����������������������
        bytes_read = fread(&data_to_read, 1, 2, file);
        if (bytes_read != 2) {
            break;
        }

        // ����һ�������е�����λ
        for (int bit_pos = 15; bit_pos >= 0; bit_pos--) {
            // ��鵱ǰλ�Ƿ�ѡ��
            if (select_to_extract[current_bit_index]) {
                // ��ȡ��ǰλ����ӵ�����ַ���
                uint8_t current_bit = (data_to_read >> bit_pos) & 0x01;
                char_to_out = (char_to_out << 1) | current_bit;
                bit_count++;

                // ���ۻ���8λʱ���
                if (bit_count == 8) {
                    if (mode == 0) {
                        // �ַ�ģʽ��ֱ������ַ�
                        fwrite(&char_to_out, 1, 1, file_output);
                    } else {
                        // ������ģʽ������������ַ���
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

            // �ƶ�����һ��λ����
            current_bit_index--;
            if (current_bit_index < 0) {
                current_bit_index = total_bits - 1;  // ���õ����λ
            }
        }
    }

    // ����ʣ��δ��8λ�Ĳ���
    if (bit_count > 0) {
        // �����ʣ��λ
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
        printf("����: �� %d λδ��8λ������0���\n", 8 - bit_count);
    }

    fclose(file_output);
    printf("������ȡ��ɣ�����ѱ��浽 %s\n", output_filename);
}

int main(int argc, char *argv[]) {

    printf("\n-= WAV��Ƶbit��ȡ���� v0.1 By Vantler =-\n");

    if (argc != 3) {
        printf("�÷�: %s <wav�ļ�> <����ļ�>\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    FILE *file = fopen(input_filename, "rb");
    if (!file) {
        printf("�޷���WAV�ļ�: %s\n", input_filename);
        return 1;
    }

    WavHeader header;

    // ��ȡWAV�ļ�ͷ
    size_t bytes_read = fread(&header, 1, sizeof(WavHeader), file);
    if (bytes_read != sizeof(WavHeader)) {
        printf("��ȡ�ļ�ͷʧ��\n");
        fclose(file);
        return 1;
    }

    // ��֤�ļ���ʽ
    if (memcmp(header.chunk_id, "RIFF", 4) != 0 ||
        memcmp(header.format, "WAVE", 4) != 0) {
        printf("������Ч��WAV�ļ�\n");
        fclose(file);
        return 1;
    }

    // ���fmt���С����16����Ҫ���������ֽ�
    if (header.subchunk1_size > 16) {
        fseek(file, header.subchunk1_size - 16, SEEK_CUR);
    }

    // ����data�飨��ЩWAV�ļ���������������fmt��data֮�䣩
    while (1) {
        bytes_read = fread(subchunk2_id, 1, 4, file);
        bytes_read += fread(&subchunk2_size, 1, 4, file);

        if (bytes_read != 8) {
            printf("�Ҳ���data��\n");
            fclose(file);
            return 1;
        }

        if (memcmp(subchunk2_id, "data", 4) == 0) {
            break;
        } else {
            // ���������
            fseek(file, subchunk2_size, SEEK_CUR);
        }
    }

    print_wav_info(&header);
    if (header.audio_format != 1) {
        printf("����֧�ֵ���Ƶ���ݸ�ʽ %d (��PCM)���޷���ȡ����\n", header.audio_format);
        return 0;
    }

    // ����wav����������λ������ʾ��ʾ����ȡ����ȡĿ��
    print_to_select(&header, 0);
    int input = 0;
    printf("\n������Ҫ��ȡ��λ���� (��Χ: 0 - %d)\n", header.num_channels * header.bits_per_sample - 1);
    printf("���뷶Χ����������ֽ���ѡ��\n");
    while (1) {
        printf("����: ");
        if (scanf("%d", &input) != 1) {
            // ������뻺����
            while (getchar() != '\n');
            printf("������Ч������������\n");
            continue;
        }
        // ����Ƿ��������
        if (input < 0 || input > header.num_channels * header.bits_per_sample - 1) {
            printf("�����������\n");
            break;
        }
        // ���ö�Ӧ��ѡ��λ
        select_to_extract[input] ^= 1;
        print_to_select(&header, 1);
        printf("\n");
    }

    int mode = 0;
    printf("\n��ѡ���������ģʽ��0Ϊ�ַ�ģʽ��1Ϊ������ģʽ\n");
    while (1) {
        printf("ģʽ: ");
        if (scanf("%d", &mode) != 1) {
            // ������뻺����
            while (getchar() != '\n');
            printf("������Ч������������\n");
            continue;
        }
        // ����Ƿ��������
        if (mode == 0 || mode == 1) {
            printf("��ѡ��%sģʽ\n", mode ? "������" : "�ַ�");
            break;
        }
        // ���ö�Ӧ��ѡ��λ
        printf("�������������0��1\n");
    }

    // ���ݲ�ͬ�ĳ��Ƚ�ѹ����
    switch(header.bits_per_sample) {
    case 8:
        extract_8(file, &header, mode, output_filename);
        break;
    case 16:
        extract_16(file, &header, mode, output_filename);
        break;
    default:
        printf("����֧�ֵ�λ���: %d bits\n", header.bits_per_sample);
        break;
    }

    fclose(file);
    return 0;
}
