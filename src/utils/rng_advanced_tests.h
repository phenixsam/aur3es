/*
 * ============================================================================
 * GOLDEN RNG - AVX-512 SIMD, KAT TESTS & UNIT TESTS IMPLEMENTATION
 * ============================================================================
 * Implementaciones avanzadas:
 * - SIMD con intrínsecos AVX-512/AVX2/SSE2
 * - Known Answer Tests (KAT)
 * - Tests Unitarios Completos
 * - Verificación de Determinismo
 * - Tests Estadísticos Avanzados (Diffusion, Correlation, Avalanche, etc.)
 * - Test de Simulación Monte Carlo
 * ============================================================================
 */

#ifndef RNG_ADVANCED_TESTS_H
#define RNG_ADVANCED_TESTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declaration */
struct rng_engine;
typedef struct rng_engine golden_rng_t;

/* ============================================================================
 * CONFIGURACIÓN DE TESTS
 * ============================================================================ */

#define TEST_SEED "GoldenRNG-Test-2024"
#define KAT_ITERATIONS 10
#define UNIT_TEST_SAMPLES 100000

/* ============================================================================
 * KAT - KNOWN ANSWER TESTS
 * ============================================================================ */

/**
 * @brief Resultado de tests KAT
 */
typedef struct {
    int total_tests;
    int passed;
    int failed;
    const char *description;
} kat_result_t;

/**
 * @brief Vector de prueba KAT para AUR3ES
 */
typedef struct {
    const char *name;
    uint8_t seed[32];
    uint64_t expected_output[KAT_ITERATIONS];
} kat_vector_t;

/**
 * @brief Ejecutar Known Answer Tests
 * @return Resultado de los tests
 */
kat_result_t run_kat_tests(void);

/**
 * @brief Verificar un vector KAT específico
 * @param vector Vector de prueba
 * @return 1 si pasa, 0 si falla
 */
int verify_kat_vector(const kat_vector_t *vector);

/**
 * @brief Imprimir reporte KAT formateado
 * @param result Resultado a imprimir
 */
void kat_print_report(const kat_result_t *result);

/**
 * @brief Generar vectores KAT de referencia
 * @param seed Semilla base
 * @param count Número de vectores a generar
 */
void generate_kat_vectors(uint8_t *seed, int count);

/* ============================================================================
 * UNIT TESTS - SUITE COMPLETA
 * ============================================================================ */

/**
 * @brief Resultado de suite de tests unitarios
 */
typedef struct {
    const char *suite_name;
    int total_tests;
    int passed;
    int failed;
    int skipped;
    double total_time_ms;
} unit_suite_result_t;

/**
 * @brief Resultado de todos los tests
 */
typedef struct {
    unit_suite_result_t core;
    unit_suite_result_t statistical;
    unit_suite_result_t deterministic;
    unit_suite_result_t simd;
    int total_passed;
    int total_failed;
    int total_time_ms;
} unit_test_result_t;

/**
 * @brief Suite: Tests de funciones core (rotl64, mix64, etc.)
 */
unit_suite_result_t run_core_tests(void);

/**
 * @brief Suite: Tests de distribuciones estadísticas
 */
unit_suite_result_t run_statistical_tests(void);

/**
 * @brief Suite: Tests de determinismo
 */
unit_suite_result_t run_deterministic_tests(void);

/**
 * @brief Suite: Tests de SIMD
 */
unit_suite_result_t run_simd_tests(void);

/**
 * @brief Ejecutar todas las suites de tests
 */
unit_test_result_t run_all_unit_tests(void);

/**
 * @brief Imprimir reporte de tests unitarios
 */
void unit_test_print_report(const unit_test_result_t *result);

/* ============================================================================
 * VERIFICACIÓN DE DETERMINISMO
 * ============================================================================ */

/**
 * @brief Resultado de test de determinismo
 */
typedef struct {
    int iterations;
    int passed;
    int failed;
    bool is_deterministic;
    uint64_t first_value;
    uint64_t last_value;
} determinism_result_t;

/**
 * @brief Verificar que el RNG produce los mismos resultados con la misma semilla
 * @param seed Semilla de prueba
 * @param seed_len Longitud de la semilla
 * @param iterations Número de iteraciones
 * @return Resultado del test
 */
determinism_result_t verify_determinism(const uint8_t *seed, size_t seed_len, int iterations);

/**
 * @brief Test de estabilidad de secuencia
 * @param rng Manejador del RNG
 * @return true si la secuencia es estable
 */
bool test_sequence_stability(void);

/**
 * @brief Comparar dos secuencias de números aleatorios
 * @param seq1 Primera secuencia
 * @param seq2 Segunda secuencia
 * @param count Número de elementos
 * @return true si son idénticas
 */
bool compare_sequences(const uint64_t *seq1, const uint64_t *seq2, size_t count);

/**
 * @brief Imprimir reporte de determinismo
 */
void determinism_print_report(const determinism_result_t *result);

/* ============================================================================
 * TESTS ESTADÍSTICOS AVANZADOS
 * ============================================================================ */

/**
 * @brief Resultado del test de efecto avalancha
 */
typedef struct {
    int num_samples;
    double avg_bit_changes;
    int min_changes;
    int max_changes;
    double std_dev;
    int passed;
} avalanche_result_t;

/**
 * @brief Resultado del test de difusión
 */
typedef struct {
    int num_iterations;
    double avg_diffusion;
    double min_diffusion;
    double max_diffusion;
    double std_dev;
    int passed;
    int failed_bits;
} diffusion_result_t;

/**
 * @brief Resultado del test de correlación
 */
typedef struct {
    int num_samples;
    double correlation;
    double abs_correlation;
    int passed;
} correlation_result_t;

/**
 * @brief Resultado del test de autocorrelación
 */
typedef struct {
    int num_samples;
    int max_lag;
    double *correlations;
    int failed_lags;
    int passed;
} autocorr_result_t;

/**
 * @brief Resultado del test serial
 */
typedef struct {
    int num_blocks;
    int block_size;
    int counts[4];
    double chi_square;
    int passed;
} serial_result_t;

/**
 * @brief Resultado del test de rachas
 */
typedef struct {
    int num_samples;
    int total_bits;
    int ones_count;
    double rate;
    double pi_estimate;
    double pi_error;
    int passed;
} runs_result_t;

/**
 * @brief Resultado del test de frecuencia
 */
typedef struct {
    int num_samples;
    int ones_count;
    double proportion;
    double chi_square;
    int passed;
} frequency_result_t;

/**
 * @brief Resultado del test de frecuencia por bloques
 */
typedef struct {
    int num_blocks;
    int block_size;
    double chi_square;
    int passed;
} block_freq_result_t;

/**
 * @brief Resultado del test de rango de matriz binaria
 */
typedef struct {
    int num_matrices;
    int full_rank_count;
    int rank_31_count;
    int less_rank_count;
    double chi_square;
    int passed;
} matrix_rank_result_t;

/**
 * @brief Resultado del test de entropía
 */
typedef struct {
    int num_samples;
    double entropy;
    int min_byte_count;
    int max_byte_count;
    int passed;
} entropy_result_t;

/**
 * @brief Resultado de todos los tests avanzados
 */
typedef struct {
    int total_tests;
    int passed;
    int failed;
    avalanche_result_t avalanche;
    diffusion_result_t diffusion;
    correlation_result_t correlation;
    autocorr_result_t autocorr;
    serial_result_t serial;
    runs_result_t runs;
    frequency_result_t frequency;
    block_freq_result_t block_freq;
    matrix_rank_result_t matrix_rank;
    entropy_result_t entropy;
} advanced_test_result_t;

/**
 * @brief Test de efecto avalancha
 * @param rng Manejador del RNG
 * @param num_samples Número de muestras
 * @return Resultado del test
 */
avalanche_result_t test_avalanche_effect(void *rng, int num_samples);

/**
 * @brief Imprimir reporte de efecto avalancha
 */
void avalanche_print_report(const avalanche_result_t *result);

/**
 * @brief Test de difusión
 * @param rng Manejador del RNG
 * @param num_iterations Número de iteraciones
 * @return Resultado del test
 */
diffusion_result_t test_diffusion(void *rng, int num_iterations);

/**
 * @brief Imprimir reporte de difusión
 */
void diffusion_print_report(const diffusion_result_t *result);

/**
 * @brief Test de correlación
 * @param rng Manejador del RNG
 * @param num_samples Número de muestras
 * @return Resultado del test
 */
correlation_result_t test_correlation(void *rng, int num_samples);

/**
 * @brief Imprimir reporte de correlación
 */
void correlation_print_report(const correlation_result_t *result);

/**
 * @brief Test de autocorrelación
 * @param rng Manejador del RNG
 * @param num_samples Número de muestras
 * @param max_lag Máximo lag a probar
 * @return Resultado del test
 */
autocorr_result_t test_autocorrelation(void *rng, int num_samples, int max_lag);

/**
 * @brief Imprimir reporte de autocorrelación
 */
void autocorr_print_report(const autocorr_result_t *result);

/**
 * @brief Liberar resultado de autocorrelación
 */
void autocorr_free_result(autocorr_result_t *result);

/**
 * @brief Test serial (NIST)
 * @param rng Manejador del RNG
 * @param num_blocks Número de bloques
 * @param block_size Tamaño de bloque
 * @return Resultado del test
 */
serial_result_t test_serial(void *rng, int num_blocks, int block_size);

/**
 * @brief Imprimir reporte serial
 */
void serial_print_report(const serial_result_t *result);

/**
 * @brief Test de rachas
 * @param rng Manejador del RNG
 * @param num_samples Número de muestras
 * @return Resultado del test
 */
runs_result_t test_runs(void *rng, int num_samples);

/**
 * @brief Imprimir reporte de rachas
 */
void runs_print_report(const runs_result_t *result);

/**
 * @brief Test de frecuencia (monobit)
 * @param rng Manejador del RNG
 * @param num_samples Número de muestras
 * @return Resultado del test
 */
frequency_result_t test_frequency(void *rng, int num_samples);

/**
 * @brief Imprimir reporte de frecuencia
 */
void frequency_print_report(const frequency_result_t *result);

/**
 * @brief Test de frecuencia por bloques
 * @param rng Manejador del RNG
 * @param num_blocks Número de bloques
 * @param block_size Tamaño de bloque
 * @return Resultado del test
 */
block_freq_result_t test_block_frequency(void *rng, int num_blocks, int block_size);

/**
 * @brief Imprimir reporte de frecuencia por bloques
 */
void block_freq_print_report(const block_freq_result_t *result);

/**
 * @brief Test de rango de matriz binaria
 * @param rng Manejador del RNG
 * @param num_matrices Número de matrices
 * @return Resultado del test
 */
matrix_rank_result_t test_binary_matrix_rank(void *rng, int num_matrices);

/**
 * @brief Imprimir reporte de rango de matriz
 */
void matrix_rank_print_report(const matrix_rank_result_t *result);

/**
 * @brief Test de entropía de Shannon
 * @param rng Manejador del RNG
 * @param num_samples Número de muestras
 * @return Resultado del test
 */
entropy_result_t test_entropy(void *rng, int num_samples);

/**
 * @brief Imprimir reporte de entropía
 */
void entropy_print_report(const entropy_result_t *result);

/**
 * @brief Ejecutar todos los tests avanzados
 * @param rng Manejador del RNG
 * @return Resultado agregado de todos los tests
 */
advanced_test_result_t run_advanced_tests(void *rng);

/**
 * @brief Imprimir resumen de tests avanzados
 */
void advanced_test_print_summary(const advanced_test_result_t *result);

/* ============================================================================
 * SIMD CON INTRÍNSECOS
 * ============================================================================ */

/**
 * @brief Generar bytes usando AVX-512
 * @param rng Manejador del RNG
 * @param buffer Buffer de salida
 * @param size Tamaño en bytes (debe ser múltiplo de 64)
 */
void rng_generate_bytes_avx512(void *rng, unsigned char *buffer, size_t size);

/**
 * @brief Generar bytes usando AVX2
 * @param rng Manejador del RNG
 * @param buffer Buffer de salida
 * @param size Tamaño en bytes (debe ser múltiplo de 32)
 */
void rng_generate_bytes_avx2(void *rng, unsigned char *buffer, size_t size);

/**
 * @brief Generar bytes usando SSE2
 * @param rng Manejador del RNG
 * @param buffer Buffer de salida
 * @param size Tamaño en bytes (debe ser múltiplo de 16)
 */
void rng_generate_bytes_sse2(void *rng, unsigned char *buffer, size_t size);

/**
 * @brief Seleccionar automáticamente la mejor implementación SIMD
 * @param rng Manejador del RNG
 * @param buffer Buffer de salida
 * @param size Tamaño en bytes
 */
void rng_generate_bytes_simd_auto(void *rng, unsigned char *buffer, size_t size);

/**
 * @brief Obtener el mejor método SIMD disponible
 * @return String con el nombre del método
 */
const char* get_best_simd_method(void);

/**
 * @brief Benchmark comparando todos los métodos SIMD
 */
void benchmark_simd_methods(void);

/* ============================================================================
 * UTILIDADES DE TESTING
 * ============================================================================ */

/**
 * @brief Calcular media de un array de doubles
 */
double calculate_mean(const double *values, size_t count);

/**
 * @brief Calcular desviación estándar
 */
double calculate_stddev(const double *values, size_t count, double mean);

/**
 * @brief Calcular chi-cuadrado para distribución uniforme
 */
double calculate_chi_squared(const int *bins, int num_bins, int total_samples);

/**
 * @brief Verificar que no hay NaN o Inf
 * @param values Array de doubles
 * @param count Número de elementos
 * @return true si todos son válidos
 */
bool validate_doubles(const double *values, size_t count);

/**
 * @brief Inicializar RNG para tests (semilla fija)
 */
void* test_rng_create(void);

/**
 * @brief Destruir RNG de tests
 */
void test_rng_destroy(void *rng);

/* ============================================================================
 * MONTE CARLO SIMULATION TEST
 * ============================================================================ */

/**
 * @brief Resultado del test de simulación Monte Carlo
 */
typedef struct {
    uint64_t total_points;
    uint64_t points_inside_circle;
    double pi_estimate;
    double pi_error;
    double elapsed_time;
    double points_per_second;
    int passed;
} monte_carlo_result_t;

/**
 * @brief Ejecutar simulación Monte Carlo para estimar Pi
 * @param rng Manejador del RNG
 * @param num_points Número de puntos a generar
 * @return Resultado de la simulación
 */
monte_carlo_result_t test_monte_carlo_pi(void *rng, uint64_t num_points);

/**
 * @brief Imprimir reporte de Monte Carlo
 */
void monte_carlo_print_report(const monte_carlo_result_t *result);

/**
 * @brief Ejecutar test de Monte Carlo con diferentes configuraciones
 * @param rng Manejador del RNG
 * @return Resultado agregado
 */
monte_carlo_result_t run_monte_carlo_tests(void *rng);

#endif /* RNG_ADVANCED_TESTS_H */

