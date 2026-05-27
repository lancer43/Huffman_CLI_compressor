#define _CRT_SECURE_NO_WARNINGS

#include "interface.h"
#include <locale.h>

/*
    Готова первая версия упаковки битов в файл compress_file_v1()

    СДЕЛАНО!!!!!!!! 
    Требуется сделать соответствующий данному формату упаковки декомпрессор

    СДЕЛАНО!!!!!!!!
    ДАЛЕЕ ЗАЛИВАЕМ НА git наш MVP

    Дополнительные фичи (перспективы проекта):
    - сделать интерфейс для вызова компрессии/декомпрессии, и возможно анализа времени работы, коэффициента сжатия и т.д.

    - сделать вторую версию упаковки, чтобы вместо оверхеда 2кб из массива частот
    был оверхед из бинарной записи готового дерева хаффмана, сделать соответствующее
    этой упаковке алгоритм декомпресса

    - запустить тест упаковки и распаковки 50-100 рандомных файлов с ПК с проверкой
    на соответствие исходник == распакованный
*/

/* бывший main (пока пусть тут полежит)
    setlocale(LC_ALL, "Russian");
    printf("====== Проверка сжатия файла ======");
    
    FILE* source = fopen("warandpeace.txt", "rb");
    FILE* compressed_file = fopen("compressed.hf", "wb");

    if (source == NULL) {
        printf("\n\nОшибка открытия исходного файла (для чтения)");
        return 1;
    }
    if (compressed_file == NULL) {
        printf("\n\nОшибка открытия сжатого файла (для записи)");
        return 1;
    }

    printf("\n\nИсходный файл: warandpeace.txt\nСжатый файл: compressed.hf\nРаспакованный файл: decompressed.txt");

    printf("\n\nПодсчёт частоты каждого символа...");
    size_t freq_count[ASCII_ALP_SIZE] = { 0 };
    int success = frequency_counting(source, freq_count);
    if (!success) {
        printf("\nПодсчет частоты неудачный");
        return 1;
    }
    printf("\nЧастота посчитана успешно!");

    CodeTable table = { 0 };

    printf("\n\nЗаполняем таблицу бинарных кодов для сжатия...");
    success = coding_symbols(freq_count, &table);
    if (!success) {
        printf("\nОшибка заполнения таблицы кодов");
    }
    printf("\nТаблица кодов успешно заполнена!");
    
    // размер файла в байтах
    size_t source_size = ftell(source);
    fseek(source, 0, SEEK_SET);

    printf("\n\nСжимаем файл...");
    success = compress_file_v1(source, compressed_file, freq_count, &table);
    if (!success) {
        printf("\nОшибка сжатия файла");
        return 1;
    }
    printf("\nФайл сжат успешно!");

    // перед восстановлением каретки записываем длину в байтах
    fseek(compressed_file, 0, SEEK_END);
    size_t compressed_size = ftell(compressed_file);
    fseek(compressed_file, 0, SEEK_SET);

    fclose(source);
    fclose(compressed_file);
    

    compressed_file = fopen("compressed.hf", "rb");
    FILE* decompressed_file = fopen("decompressed.txt", "wb");

    if (compressed_file == NULL) {
        printf("\n\nОшибка открытия сжатого файла (для чтения)");
        return 1;
    }
    if (decompressed_file == NULL) {
        printf("\n\nОшибка открытия распакованного файла (для записи)");
        return 1;
    }

    printf("\n\nРаспаковываем файл...");
    success = decompress_file_v1(compressed_file, decompressed_file);
    if (!success) {
        printf("\nОшибка распаковки файла");
        return 1;
    }
    printf("\nФайл распакован успешно!");

    fclose(compressed_file);
    fclose(decompressed_file);

    source = fopen("warandpeace.txt", "rb");
    decompressed_file = fopen("decompressed.txt", "rb");

    fseek(source, 0, SEEK_SET);
    fseek(decompressed_file, 0, SEEK_SET);

    if (source == NULL) {
        printf("\n\nОшибка открытия сжатого файла (для чтения)");
        return 1;
    }
    if (decompressed_file == NULL) {
        printf("\n\nОшибка открытия распакованного файла (для чтения)");
        return 1;
    }

    printf("\n\nПроверка корректности распаковки...");
    int s_ch = 0, d_ch = 0;

    // Читаем оба файла до тех пор, пока ХОТЯ БЫ ОДИН из них не кончится
    while (1) {
        s_ch = fgetc(source);
        d_ch = fgetc(decompressed_file);

        // Если кто-то один кончился или символы не совпали — выходим из цикла
        if (s_ch == EOF || d_ch == EOF || s_ch != d_ch) {
            break;
        }
    }

    // Если они не равны — значит, либо символы внутри разные, либо один файл длиннее
    if (s_ch != d_ch) {
        long pos = ftell(source);
        printf("\nОшибка: файлы не совпадают!");
        if (s_ch == EOF) printf("\nРаспакованный файл длиннее исходного!");
        if (d_ch == EOF) printf("\nИсходный файл длиннее распакованного!");
        if (s_ch != EOF && d_ch != EOF) printf("\nРасхождение на позиции %ld (символы: '%c' vs '%c')", pos, s_ch, d_ch);

        fclose(source);
        fclose(decompressed_file);
        return 1;
    }

    printf("\n\nФайлы совпадают!");

    printf("\n\n ====== Анализ эффективности программы ======");
    printf("\n\nВес исходного файла: %zu байт", source_size);
    printf("\nВес сжатого файла: %zu байт", compressed_size);

    if (source_size > compressed_size) {
        double coefficient = (double)source_size / compressed_size;
        printf("\n\nВ памяти сэкономлено: %zu байт", source_size - compressed_size);
        printf("\nКоэффициент сжатия равен %.2lf", coefficient);
    }
    else {
        printf("\nК сожалению, программа оказалась неэффективна :(");
    }
*/

int main(void) {
    setlocale(LC_ALL, "Russian");

    run_interface_huf();

    return 0;
}