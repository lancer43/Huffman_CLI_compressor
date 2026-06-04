#define _CRT_SECURE_NO_WARNINGS
#include "interface.h"

// модули которыми мы управляем
#include "compressor.h"
#include "decompressor.h"
#include "autotest.h"

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>




static void greeting_user(void) {
	printf("========== АРХИВАТОР ЗАПУЩЕН ==========");
	printf("\n\nДобро пожаловать в архиватор!");
}

static void show_menu(void) {
	printf("\n\n================ МЕНЮ ================");

	printf("\n\nВыберите действие:");
	printf("\n1. Сжать файл");
	printf("\n2. Распаковать файл");
	printf("\n3. Запуск автотеста");
	printf("\n0. Выход");
}
/*
	@brief Вывод анализа работы архиватора
	@param stats - указатель на структуру с собранной статистикой
	@param mode - режим работы архиватора (сжатие/распаковка)
*/
static void show_analysis(AlgorithmEfficiency_s* stats, const Mode_e mode) {
	assert(mode == RUN_COMPRESS || mode == RUN_DECOMPRESS);
	assert(stats != NULL);
	
	switch (mode) {
	case RUN_COMPRESS:
		printf("\n\n========== АНАЛИЗ СЖАТИЯ ФАЙЛА ==========");

		printf("\n\nРазмер исходного файла: %zu байт", stats->in_size);
		printf("\nРазмер сжатого файла: %zu байт", stats->out_size);
		printf("\nКоэффициент сжатия файла равен %.2lf", stats->coefficient);
		
		printf("\n\nОбщее время работы алгоритма составило %.3lf сек.", stats->total_time);
		printf("\n\t- Подсчёт частот занял %.3lf сек.", stats->freq_time);
		printf("\n\t- Заполнение таблицы бинарных кодов заняло %.3lf сек.", stats->bincode_time);
		printf("\n\t- Сжатие файла заняло %.3lf сек.", stats->compress_time);
		break;

	case RUN_DECOMPRESS:
		printf("\n\n========== АНАЛИЗ РАСПАКОВКИ ФАЙЛА ==========");

		printf("\n\nРазмер сжатого файла: %zu байт", stats->in_size);
		printf("\nРазмер распакованного файла: %zu байт", stats->out_size);
		printf("\nКоэффициент сжатия файла равен %.2lf", stats->coefficient);

		printf("\n\nВремя распаковки файла составило %.3lf сек.", stats->total_time);
		break;
	}
}



/*
	@brief Ввод пути файла для сжатия/распаковки
	@param input_path - строка для сохранения корректного пути
	@return возвращает флаг расширения файла (есть - 1, нет - 0)
*/
static PathStatus_e enter_path(char input_path[MAX_PATH_LEN]) {
	// главной функцией run_interface_huf() гарантируется строка-путь нужной длины
	assert(input_path != NULL);
	int valid_flag = 0;
	int extension_flag = -1;

	while (!valid_flag) {
		extension_flag = -1;
		printf("\nПеретащите файл в консоль (или введите путь к файлу вручную): ");

		// fgets() читает максимум MAX_PATH_LEN - 1 символов
		if (fgets(input_path, MAX_PATH_LEN, stdin) == NULL) {
			printf("\nОшибка чтения. Повторите попытку.");

			// чистим буфер
			int c;
			while ((c = getchar()) != '\n' && c != EOF);

			continue;
		}

		// если длинный путь
		if (strchr(input_path, '\n') == NULL && strlen(input_path) == MAX_PATH_LEN - 1) {
			printf("\nОшибка: путь слишком длинный. Повторите попытку");

			// чистим буфер
			int c;
			while ((c = getchar()) != '\n' && c != EOF);

			continue;
		}

		// если просто нажали Enter
		if (input_path[0] == '\n') {
			printf("\nПустой ввод. Повторите попытку.");
		}

		// заменяем последний символ на конец строки
		input_path[strcspn(input_path, "\n")] = '\0';

		// вспомогательный указатель
		char* ptr = NULL;

		// заменяем блок очистки пробелов, кавычек и слэшей:
		char* read_ptr = input_path;
		char* write_ptr = input_path;

		// пропускаем ведущие пробелы
		while (*read_ptr == ' ') read_ptr++;

		// копируем строку, на лету выкидывая кавычки и меняя слэши
		while (*read_ptr != '\0') {
			if (*read_ptr == '\"') {
				read_ptr++; // игнорируем кавычку
			}
			else {
				if (*read_ptr == '\\') {
					*write_ptr = '/'; // исправляем слэш
				}
				else {
					*write_ptr = *read_ptr; // копируем символ
				}
				write_ptr++;
				read_ptr++;
			}
		}
		*write_ptr = '\0'; // закрываем строку

		// отрезаем концевые пробелы (безопасно, без выхода за границы)
		if (write_ptr > input_path) {
			write_ptr--; // Встаем на последний символ перед '\0'
			while (write_ptr >= input_path && *write_ptr == ' ') {
				*write_ptr-- = '\0';
			}
		}

		// если введен просто текст
		char* format_point = strrchr(input_path, '.');
		ptr = strrchr(input_path, '/');
		
		if (format_point == NULL ||
			(ptr != NULL && format_point < ptr) ||
			format_point == ptr + 1 ||
			format_point == input_path) {
			printf("\nПредупреждение: формат файла неопределён. Отправьте любой символ, если хотите повторить ввод (иначе - Enter).\n");
			int c = getchar();

			if (c != '\n') {

				while ((c = getchar()) != '\n' && c != EOF);
				continue;
			}
			extension_flag = PATH_VALID_NO_EXTENSION;
		}

		// финальная железобетонная проверка
		FILE* test_open = fopen(input_path, "rb");
		if (test_open == NULL) {
			printf("\nДанный путь не существует. Повторите попытку.");
			continue;
		}

		fclose(test_open);

		valid_flag = 1;
		if (extension_flag == -1) {
			extension_flag = PATH_VALID_WITH_EXTENSION;
		}
	}
	return extension_flag;
}

/*
	@brief Перезапись имени файла
	@param path - путь к файлу
	@param extension_flag - флаг о наличии расширения файла (есть/нет)
*/
static void rewrite_file_name(char* path) {
	int c;

	printf("\nВведите новое имя файла: ");
	char new_file_name[MAX_PATH_LEN];

	int flag = 0;

	while (!flag) {
		if (fgets(new_file_name, MAX_PATH_LEN, stdin) == NULL) {
			printf("\nОшибка чтения. Повторите попытку: ");

			// чистим буфер
			while ((c = getchar()) != '\n' && c != EOF);
			continue;
		}

		// если пользователь просто нажал Enter из-за старого буфера, пропускаем
		if (new_file_name[0] == '\n') {
			continue;
		}

		new_file_name[strcspn(new_file_name, "\n")] = '\0';

		// извлекаем текущее расширение файла (.huf или любое другое)
		char extension[MAX_PATH_LEN] = "\0";
		char* ext_ptr = strrchr(path, '.');
		if (ext_ptr != NULL) {

			int written = snprintf(extension, sizeof(extension), "%s", ext_ptr);
			if (written < 0 || written > sizeof(extension)) {
				printf("\nОшибка записи расширения во вспомогательный массив.");
			}
		}

		// ищем последний слэш в текущем пути
		char* slash_ptr = strrchr(path, '/');
		char dir_path[MAX_PATH_LEN] = "\0";

		if (slash_ptr != NULL) {
			size_t dir_len = slash_ptr - path + 1;
			if (dir_len < MAX_PATH_LEN) {
				
				int written = snprintf(dir_path, dir_len + 1, "%s", path);
				if (written < 0 || written > sizeof(extension)) {
					printf("\nОшибка записи директории во вспомогательный массив.");
				}
			}
		}

		// собираем новый путь с сохраненным расширением
		char temp_path[MAX_PATH_LEN];
		int written = snprintf(temp_path, MAX_PATH_LEN, "%s%s%s", dir_path, new_file_name, extension);

		if (written < 0 || written >= MAX_PATH_LEN) {
			printf("\nОшибка: итоговый путь слишком длинный. Повторите попытку: ");
			continue;
		}

		// перезаписываем исходный путь
		written = snprintf(path, MAX_PATH_LEN, "%s", temp_path);

		if (written < 0 || written > MAX_PATH_LEN) {
			printf("\nОшибка записи нового пути к файлу.");
		}

		flag = 1;
	}
}

/*
	@brief Проверка существования файла по заданному пути
	@param path - путь
	@return 1 - проблема решена, 0 - отмена выполнения
*/
static int check_existence_file(char* path, PathStatus_e extension_flag) { // [TODO] зациклить при повторно неверном вводе
	FILE* test_file = fopen(path, "rb");

	// если файл не открылся — значит его нет, путь свободен для записи
	if (test_file == NULL) {
		return 1;
	}
	fclose(test_file);

	printf("\nСжатый файл с таким именем уже существует. Хотите перезаписать его?\n");
	printf("[y - перезаписать/n - отменить/r - изменить имя файла]: ");

	int choice = -1;
	int flag = 0;
	int res = 0; // возвращаемое значение
	int c; // переменная для очистки буфера

	while (!flag) {
		choice = getchar();

		switch (choice) {
		case 'y':
		case 'Y':
			flag = 1;
			res = 1;
			// чистим буфер от оставшегося '\n'
			while ((c = getchar()) != '\n' && c != EOF);
			break;
		case 'n':
		case 'N':
			flag = 1;
			res = 0;
			// чистим буфер от оставшегося '\n'
			while ((c = getchar()) != '\n' && c != EOF);
			break;
		case 'r':
		case 'R':
			// чистим буфер от оставшегося '\n'
			while ((c = getchar()) != '\n' && c != EOF);

			rewrite_file_name(path);
			flag = 1;
			res = 1;
			break;
		default:
			printf("\nНекорректный ввод. Повторите попытку: ");
			
			// чистим буфер от оставшегося '\n'
			while ((c = getchar()) != '\n' && c != EOF);
			break;
		}
	}

	return res;
}

/*
	@brief Интерфейс сжатия файла.
	@brief Вызывается ввод пути к исходному файлу от пользователя, автоматически формируется путь для сжатого файла,
	запускается цикл сжатия файла, выводится аналитика (время каждой стадии цикла сжатия, размеры файлов, коэффициент сжатия).
	@brief [ПРИМЕЧАНИЕ] Если путь для сжатого файла занят, то пользователю предоставляется возможность выбрать действие с путём.
*/
static void compress_interface(void) {
	// ввод пути к исходному файлу
	char source_path[MAX_PATH_LEN];
	int extension_flag = enter_path(source_path); // узнаем есть ли у него расширение

	// создаём путь для записи сжатого файла
	char compress_path[MAX_PATH_LEN];
	create_compress_path(source_path, compress_path, extension_flag);

	// проверяем существует ли такой сжатый файл и надо ли его перезаписать
	if (!check_existence_file(compress_path, extension_flag)) return;
	
	// выделяем структуру под данные для аналитики
	AlgorithmEfficiency_s stats_compress = { 0 };

	// запускаем цикл сжатия файла
	run_compress(source_path, compress_path, &stats_compress, extension_flag);

	// выводим аналитику на экран
	show_analysis(&stats_compress, RUN_COMPRESS);
}

/*
	@brief Интерфейс распаковки файла.
	@brief Вызывается ввод пути к сжатому файлу от пользователя, автоматически формируется путь для распакованного файла,
	запускается цикл распаковки файла, выводится аналитика (время цикла распаковки, размеры файлов, коэффициент сжатия).
	@brief [ПРИМЕЧАНИЕ] Если путь для распакованного файла занят, то пользователю предоставляется возможность выбрать действие с путём.
*/
static void decompress_interface(void) {
	// ввод пути к сжатому файлу
	char compress_path[MAX_PATH_LEN];
	enter_path(compress_path);

	// проверка на нужный формат
	char* point = strrchr(compress_path, '.');
	if (point == NULL || strcmp(point, COMPRESSED_EXTENSION) != 0) {
		printf("\nОшибка: неверный формат файла.");
		return;
	}
	point = NULL;

	// создаем путь для записи распакованного файла
	char decompress_path[MAX_PATH_LEN];
	create_decompress_path(compress_path, decompress_path);

	// проверяем существует ли такой распакованный файл и надо ли его перезаписать
	if (!check_existence_file(decompress_path, PATH_VALID_WITH_EXTENSION)) return;

	// выделяем структуру под данные для аналитики
	AlgorithmEfficiency_s stats_decompress = { 0 };

	// запускаем цикл сжатия файла
	run_decompress(compress_path, decompress_path, &stats_decompress);

	// выводим аналитику на экран
	show_analysis(&stats_decompress, RUN_DECOMPRESS);
}

static void autotest_interface() {




	run_autotest();
}


// ========== ГЛАВНАЯ ФУНКЦИЯ МОДУЛЯ ==========

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
		
			case RUN_COMPRESS: {
				

				compress_interface();

				break;
			}

			case RUN_DECOMPRESS: {
				

				decompress_interface();

				break;
			}

			case RUN_AUTOTEST:
				// заглушка
				srand(time(NULL));
				autotest_interface();
				break;

			case RUN_EXIT:
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