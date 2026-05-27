#define _CRT_SECURE_NO_WARNINGS
#include "interface.h"

// модули которыми мы управляем
#include "compressor.h"
#include "decompressor.h"

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <string.h>

#define MAX_PATH_LEN 1024

enum {
	exit,
	run_compress,
	run_decompress
};

static void greeting_user(void) {
	printf("========== АРХИВАТОР ЗАПУЩЕН ==========");
	printf("\n\nДобро пожаловать в архиватор!");
}

static void show_menu(void) {
	printf("\n\n================ МЕНЮ ================");

	printf("\n\nВыберите действие:");
	printf("\n1. Сжать файл\
			\n2. Распаковать файл\
			\n0. Выход");


}

static void enter_path(char* input_path) {
	printf("\nПеретащите файл в консоль (или введите путь к файлу вручную): ");

	while (fgets(input_path, MAX_PATH_LEN, stdin) == NULL) {
		printf("\nОшибка записи пути к файлу. Повторите попытку.");
		printf("\nПеретащите файл в консоль (или введите путь к файлу вручную): ");
	};

	// очищаем файл от кавычек по бокам
	char* quote_ptr = strchr(input_path, '\"');
	while (quote_ptr != NULL) {
		memmove(quote_ptr, &quote_ptr[1], strlen(quote_ptr));

		quote_ptr = strchr(input_path, '\"');
	}

	// заменяем последний символ на конец строки
	input_path[strcspn(input_path, "\n")] = '\0';
}

static void run_compress_file(const char* input_path, const char* output_path) {
	FILE* source = fopen(input_path, "rb");
	FILE* compressed_file = fopen(output_path, "wb");

	if (source == NULL) {
		printf("\n\nОшибка открытия исходного файла (для чтения)");
		return;
	}
	if (compressed_file == NULL) {
		printf("\n\nОшибка открытия сжатого файла (для записи)");
		return;
	}

	printf("\n\nПодсчёт частоты каждого символа...");
	size_t freq_count[ASCII_ALP_SIZE] = { 0 };
	int success = frequency_counting(source, freq_count);
	if (!success) {
		printf("\nПодсчет частоты неудачный");
		return;
	}
	printf("\nЧастота посчитана успешно!");

	// [DEBUG]: смотрим частоты
	/*
	for (size_t i = 0; i < ASCII_ALP_SIZE; i++) {
		printf("\n[DEBUG]: arr[%zu] = %zu", i, freq_count[i]);
	}
	*/

	CodeTable table = { 0 };

	printf("\n\nЗаполняем таблицу бинарных кодов для сжатия...");
	success = coding_symbols(freq_count, &table);
	if (!success) {
		printf("\nОшибка заполнения таблицы кодов");
	}
	printf("\nТаблица кодов успешно заполнена!");

	fseek(source, 0, SEEK_SET);

	printf("\n\nСжимаем файл...");
	success = compress_file_v1(source, compressed_file, freq_count, &table);
	if (!success) {
		printf("\nОшибка сжатия файла");
		return;
	}

	printf("\nФайл сжат успешно!");
	printf("\n\nСжатый файл лежит по пути: %s", output_path);
}

static void compress_interface(const char* input_path) {
	// создаём путь для записи сжатого файла
	char output_path[MAX_PATH_LEN];

	strncpy(output_path, input_path, MAX_PATH_LEN);
	
	char* formatter = strrchr(output_path, '.');
	// [TODO]: можно переделать проверку на корректность пути более "умно"
	if (formatter == NULL) {
		printf("Ошибка: введён некорректный формат файла");
		return;
	}

	formatter[1] = '\0';
	strncat(formatter, "huf", 3);

	// тут щас будем выделять память, таблицу все дела пупупу
	run_compress_file(input_path, output_path);
}

void run_interface_huf(void) {
	
	greeting_user();

	while (1) {
		show_menu();
		int choice = -1;
		int flag = 0;

		printf("\n\nВвод: ");
		if (scanf("%d", &choice) != 1) {
			printf("\nНеверный ввод. Повторите попытку.");

			// очистка буфера
			while (getchar() != '\n');

			continue;
		}

		// очистка буфера
		while (getchar() != '\n');

		switch (choice) {
		
		case run_compress: {
			char path[MAX_PATH_LEN];
			
			enter_path(path);

			// резервируем последние 3 символа под формат .huf
			if (strlen(path) > MAX_PATH_LEN - 3) {
				printf("\nОшибка: путь к файлу слишком длинный.");
				break;
			}

			compress_interface(path);

			break;
		}
		case run_decompress:
			// заглушка
			
			

			break;
		
		case exit:
			printf("\nВыход...");
			flag = 1;
			break;
		
		default:
			printf("\nНекорректный ввод. Повторите попытку.");
			break;
		}

		if (flag) break;
	}
}