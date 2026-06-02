#ifndef HUF_ANALYTICS

#define HUF_ANALYTICS

#include <stdio.h>
#include <time.h>


// режим запуска архиватора через интерфейс
typedef enum {
	RUN_EXIT,				// выход из программы
	RUN_COMPRESS,			// сжатие файла
	RUN_DECOMPRESS,			// распаковка файла
	RUN_AUTOTEST			// запуск автотеста
} Mode_e;

// структура для хранения данных о сжатии/распаковке файла
typedef struct {
	size_t in_size;			// размер входящего файла
	size_t out_size;		// размер выходящего файла

	double coefficient;		// коэффициент сжатия

	double total_time;		// общее время работы алгоритма
	double freq_time;		// время подсчёта частоты каждого символа
	double bincode_time;	// время заполнения таблицы бинарных кодов
	double compress_time;	// время сжатия файла
} AlgorithmEfficiency_s;

typedef struct {
	clock_t start_total;	// начало общего времени (и подсчёта частот)
	clock_t stop_freq;		// конец подсчёта частот
	clock_t start_bincode;	// начало заполнения таблицы бинарных кодов
	clock_t stop_bincode;	// конец заполнения таблицы бинарных кодов
	clock_t start_compress; // начало сжатия файла
	clock_t stop_total;		// конец общего времени (и сжатия файла)
} Clocks_s;

/*
	@brief Вычисление коэффициента сжатия файла и времени сжатия/распаковки
	@param istream - входной файл
	@param ostream - выходной файл
	@param clocks - указатель на структуру с замеренными тактами
	@param mode - режим работы (сжатие/распаковка)
	@return 1 - успех, иначе - 0
*/
int result_analysis(FILE* istream, FILE* ostream, Clocks_s* clocks, Mode_e mode, AlgorithmEfficiency_s* stats);

#endif