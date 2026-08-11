#include "app_main.h"

#include "app_log.h"
#include "app_storage_layout.h"
#include "app_version.h"
#include "bsp_board.h"
#include "bsp_i2c1.h"
#include "bsp_spi1.h"
#include "eeprom_24c02.h"
#include "FreeRTOS.h"
#include "serial_manager.h"
#include "stm32f4xx_hal.h"
#include "task.h"
#include "spi_nor.h"
#include "storage.h"
#include "storage_eeprom_24c02.h"
#include "storage_partition.h"
#include "storage_spi_nor.h"
#include "system_mode.h"
#include "update_session.h"

#include <string.h>

#define APPLICATION_FLASH_BASE 0x08040000UL

#ifndef APP_ENABLE_SPI_NOR_SELF_TEST
#define APP_ENABLE_SPI_NOR_SELF_TEST 0
#endif

#ifndef APP_ENABLE_EEPROM_SELF_TEST
#define APP_ENABLE_EEPROM_SELF_TEST 0
#endif

#define EEPROM_TRANSFER_TIMEOUT_MS 20U
#define SPI_NOR_PROGRAM_TIMEOUT_MS 500U
#define SPI_NOR_ERASE_TIMEOUT_MS   3000U
#define APP_RTOS_MIN_STACK_FREE_WORDS 64U
#define APP_HEARTBEAT_MIN_MS 100U
#define APP_HEARTBEAT_MAX_MS 10000U

#if APP_ENABLE_SPI_NOR_SELF_TEST
#define SPI_NOR_TEST_CROSS_PAGE_OFFSET 0x000000F0UL
#define SPI_NOR_TEST_LENGTH         64U

static uint8_t spi_nor_test_write_data[SPI_NOR_TEST_LENGTH];
static uint8_t spi_nor_test_read_data[SPI_NOR_TEST_LENGTH];

typedef struct
{
    bool retained_state_ok;
    bool erase_ok;
    bool program_ok;
    bool verify_ok;
    bool cross_page_ok;
    bool flash_end_ok;
    bool bounds_ok;
    bool cleanup_ok;
    bool page_boundary_rejected;
    int read_range_status;
    int write_range_status;
    int erase_alignment_status;
} app_flash_test_result_t;
#endif

#if APP_ENABLE_EEPROM_SELF_TEST
#define EEPROM_TEST_ADDRESS    0x00F6U
#define EEPROM_TEST_LENGTH     10U

static uint8_t eeprom_test_backup[EEPROM_TEST_LENGTH];
static uint8_t eeprom_test_write_data[EEPROM_TEST_LENGTH];
static uint8_t eeprom_test_read_data[EEPROM_TEST_LENGTH];

typedef struct
{
    bool backup_ok;
    bool program_ok;
    bool verify_ok;
    bool bounds_ok;
    bool restore_ok;
} app_eeprom_test_result_t;
#endif

static spi_nor_t app_flash_driver;
static eeprom_24c02_t app_eeprom_driver;
static struct storage_spi_nor app_flash_adapter;
static struct storage_eeprom_24c02 app_eeprom_adapter;
static struct storage_device app_flash_storage;
static struct storage_device app_eeprom_storage;
static struct storage_partition app_candidate_partition;
static struct storage_partition app_golden_partition;
static struct storage_partition app_metadata_a_partition;
static struct storage_partition app_metadata_b_partition;
static struct storage_partition app_driver_test_partition;
static struct storage_device app_candidate_storage;
static struct storage_device app_golden_storage;
static struct storage_device app_metadata_a_storage;
static struct storage_device app_metadata_b_storage;
static struct storage_device app_driver_test_storage;
static volatile uint32_t app_heartbeat_ms = 1000U;

static bool app_partition_bind_and_open(struct storage_device *device,
                                        const char *name,
                                        struct storage_partition *partition,
                                        uint32_t offset,
                                        uint32_t length)
{
    return (storage_partition_bind(device,
                                   name,
                                   partition,
                                   &app_flash_storage,
                                   offset,
                                   length) == STORAGE_OK) &&
           (storage_open(device) == STORAGE_OK);
}

static bool app_flash_partitions_init(void)
{
    return app_partition_bind_and_open(&app_candidate_storage,
                                       "candidate",
                                       &app_candidate_partition,
                                       APP_STORAGE_CANDIDATE_OFFSET,
                                       APP_STORAGE_CANDIDATE_SIZE) &&
           app_partition_bind_and_open(&app_golden_storage,
                                       "golden",
                                       &app_golden_partition,
                                       APP_STORAGE_GOLDEN_OFFSET,
                                       APP_STORAGE_GOLDEN_SIZE) &&
           app_partition_bind_and_open(&app_metadata_a_storage,
                                       "metadata_a",
                                       &app_metadata_a_partition,
                                       APP_STORAGE_METADATA_A_OFFSET,
                                       APP_STORAGE_METADATA_A_SIZE) &&
           app_partition_bind_and_open(&app_metadata_b_storage,
                                       "metadata_b",
                                       &app_metadata_b_partition,
                                       APP_STORAGE_METADATA_B_OFFSET,
                                       APP_STORAGE_METADATA_B_SIZE) &&
           app_partition_bind_and_open(&app_driver_test_storage,
                                       "driver_test",
                                       &app_driver_test_partition,
                                       APP_STORAGE_DRIVER_TEST_OFFSET,
                                       APP_STORAGE_DRIVER_TEST_SIZE);
}

static int app_flash_transfer(void *context,
                              const uint8_t *tx_data,
                              uint8_t *rx_data,
                              size_t length,
                              uint32_t timeout_ms)
{
    (void)context;
    return bsp_spi1_transfer(tx_data, rx_data, length, timeout_ms);
}

static void app_flash_chip_select(void *context, bool selected)
{
    (void)context;
    if (selected)
    {
        bsp_board_flash_select();
    }
    else
    {
        bsp_board_flash_deselect();
    }
}

static void app_flash_delay(void *context, uint32_t delay_ms)
{
    (void)context;
    HAL_Delay(delay_ms);
}

static int app_eeprom_is_ready(void *context,
                               uint8_t address_7bit,
                               uint32_t trials,
                               uint32_t timeout_ms)
{
    (void)context;
    return bsp_i2c1_is_device_ready(address_7bit, trials, timeout_ms);
}

static int app_eeprom_read(void *context,
                           uint8_t address_7bit,
                           uint8_t memory_address,
                           uint8_t *data,
                           size_t length,
                           uint32_t timeout_ms)
{
    (void)context;
    return bsp_i2c1_mem_read(address_7bit,
                             memory_address,
                             data,
                             length,
                             timeout_ms);
}

static int app_eeprom_write(void *context,
                            uint8_t address_7bit,
                            uint8_t memory_address,
                            const uint8_t *data,
                            size_t length,
                            uint32_t timeout_ms)
{
    (void)context;
    return bsp_i2c1_mem_write(address_7bit,
                              memory_address,
                              data,
                              length,
                              timeout_ms);
}

#if APP_ENABLE_SPI_NOR_SELF_TEST
static bool app_flash_buffer_is_erased(const uint8_t *data, size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index)
    {
        if (data[index] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}

static void app_flash_fill_test_pattern(uint8_t seed)
{
    size_t index;

    for (index = 0U; index < SPI_NOR_TEST_LENGTH; ++index)
    {
        spi_nor_test_write_data[index] = (uint8_t)(seed ^ (uint8_t)index);
    }
}

static bool app_flash_read_matches(struct storage_device *storage,
                                   uint32_t address)
{
    return (storage_read(storage,
                         address,
                         spi_nor_test_read_data,
                         sizeof(spi_nor_test_read_data)) == STORAGE_OK) &&
           (memcmp(spi_nor_test_write_data,
                   spi_nor_test_read_data,
                   sizeof(spi_nor_test_write_data)) == 0);
}

static bool app_flash_run_rw_test(struct storage_device *storage,
                                  spi_nor_t *flash,
                                  app_flash_test_result_t *result)
{
    struct storage_info info;
    uint32_t flash_end_test_address;

    memset(result, 0, sizeof(*result));
    if (storage_get_info(storage, &info) != STORAGE_OK)
    {
        return false;
    }
    flash_end_test_address = info.capacity_bytes - SPI_NOR_TEST_LENGTH;
    app_flash_fill_test_pattern(0xA5U);

    result->retained_state_ok =
        (storage_read(storage,
                      0U,
                      spi_nor_test_read_data,
                      sizeof(spi_nor_test_read_data)) == STORAGE_OK) &&
        app_flash_buffer_is_erased(spi_nor_test_read_data,
                                   sizeof(spi_nor_test_read_data));

    result->erase_ok = result->retained_state_ok &&
        (storage_erase(storage,
                       0U,
                       APP_STORAGE_DRIVER_TEST_SIZE) == STORAGE_OK) &&
                       (storage_read(storage,
                                     0U,
                                     spi_nor_test_read_data,
                                     sizeof(spi_nor_test_read_data)) == STORAGE_OK) &&
                       app_flash_buffer_is_erased(spi_nor_test_read_data,
                                                  sizeof(spi_nor_test_read_data));

    result->program_ok = result->erase_ok &&
                         spi_nor_page_program(flash,
                                              APP_STORAGE_DRIVER_TEST_OFFSET,
                                              spi_nor_test_write_data,
                                              sizeof(spi_nor_test_write_data),
                                              SPI_NOR_PROGRAM_TIMEOUT_MS);

    result->verify_ok = result->program_ok &&
                        app_flash_read_matches(storage,
                                               0U);

    app_flash_fill_test_pattern(0x5AU);
    result->cross_page_ok = result->verify_ok &&
                            (storage_write(storage,
                                           SPI_NOR_TEST_CROSS_PAGE_OFFSET,
                                           spi_nor_test_write_data,
                                           sizeof(spi_nor_test_write_data)) == STORAGE_OK) &&
                            app_flash_read_matches(storage,
                                                   SPI_NOR_TEST_CROSS_PAGE_OFFSET);

    app_flash_fill_test_pattern(0x3CU);
    result->flash_end_ok = result->cross_page_ok &&
                           (storage_write(storage,
                                          flash_end_test_address,
                                          spi_nor_test_write_data,
                                          sizeof(spi_nor_test_write_data)) == STORAGE_OK) &&
                           app_flash_read_matches(storage,
                                                  flash_end_test_address);

    result->page_boundary_rejected =
        !spi_nor_page_program(flash,
                              APP_STORAGE_DRIVER_TEST_OFFSET +
                                  SPI_NOR_TEST_CROSS_PAGE_OFFSET,
                              spi_nor_test_write_data,
                              sizeof(spi_nor_test_write_data),
                              SPI_NOR_PROGRAM_TIMEOUT_MS);
    result->read_range_status = storage_read(storage,
                                             info.capacity_bytes - 1U,
                                             spi_nor_test_read_data,
                                             2U);
    result->write_range_status = storage_write(storage,
                                               info.capacity_bytes - 1U,
                                               spi_nor_test_write_data,
                                               2U);
    result->erase_alignment_status =
        storage_erase(storage,
                      1U,
                      APP_STORAGE_DRIVER_TEST_SIZE - 1U);
    result->bounds_ok = result->flash_end_ok &&
                        result->page_boundary_rejected &&
                        (result->read_range_status == STORAGE_ERR_RANGE) &&
                        (result->write_range_status == STORAGE_ERR_RANGE) &&
                        (result->erase_alignment_status == STORAGE_ERR_INVALID);

    result->cleanup_ok =
        (storage_erase(storage,
                       0U,
                       APP_STORAGE_DRIVER_TEST_SIZE) == STORAGE_OK) &&
                         (storage_read(storage,
                                       SPI_NOR_TEST_CROSS_PAGE_OFFSET,
                                       spi_nor_test_read_data,
                                       sizeof(spi_nor_test_read_data)) == STORAGE_OK) &&
                         app_flash_buffer_is_erased(spi_nor_test_read_data,
                                                    sizeof(spi_nor_test_read_data)) &&
                         (storage_read(storage,
                                       flash_end_test_address,
                                       spi_nor_test_read_data,
                                       sizeof(spi_nor_test_read_data)) == STORAGE_OK) &&
                         app_flash_buffer_is_erased(spi_nor_test_read_data,
                                                    sizeof(spi_nor_test_read_data));

    return result->retained_state_ok && result->erase_ok &&
           result->program_ok && result->verify_ok &&
           result->cross_page_ok && result->flash_end_ok && result->bounds_ok &&
           result->cleanup_ok;
}
#endif

#if APP_ENABLE_EEPROM_SELF_TEST
static bool app_eeprom_run_rw_test(struct storage_device *storage,
                                   eeprom_24c02_t *eeprom,
                                   app_eeprom_test_result_t *result)
{
    size_t index;

    memset(result, 0, sizeof(*result));
    result->backup_ok =
        storage_read(storage,
                     EEPROM_TEST_ADDRESS,
                     eeprom_test_backup,
                     sizeof(eeprom_test_backup)) == STORAGE_OK;

    for (index = 0U; index < sizeof(eeprom_test_write_data); ++index)
    {
        eeprom_test_write_data[index] = (uint8_t)(0xA5U ^ (uint8_t)index);
    }

    result->program_ok = result->backup_ok &&
                         (storage_write(storage,
                                        EEPROM_TEST_ADDRESS,
                                        eeprom_test_write_data,
                                        sizeof(eeprom_test_write_data)) == STORAGE_OK);
    result->verify_ok = result->program_ok &&
                        (storage_read(storage,
                                      EEPROM_TEST_ADDRESS,
                                      eeprom_test_read_data,
                                      sizeof(eeprom_test_read_data)) == STORAGE_OK) &&
                        (memcmp(eeprom_test_write_data,
                                eeprom_test_read_data,
                                sizeof(eeprom_test_write_data)) == 0);
    result->bounds_ok =
        (storage_read(storage,
                      EEPROM_24C02_CAPACITY_BYTES - 1U,
                      eeprom_test_read_data,
                      2U) == STORAGE_ERR_RANGE) &&
        (storage_write(storage,
                       EEPROM_24C02_CAPACITY_BYTES - 1U,
                       eeprom_test_write_data,
                       2U) == STORAGE_ERR_RANGE) &&
        (storage_erase(storage, 0U, 1U) == STORAGE_ERR_UNSUPPORTED) &&
        !eeprom_24c02_page_write(eeprom,
                                 EEPROM_TEST_ADDRESS,
                                 eeprom_test_write_data,
                                 sizeof(eeprom_test_write_data),
                                 EEPROM_TRANSFER_TIMEOUT_MS);

    /* Always restore the original bytes after a successful backup, even when
       a preceding verification step fails. */
    result->restore_ok = result->backup_ok &&
                         (storage_write(storage,
                                        EEPROM_TEST_ADDRESS,
                                        eeprom_test_backup,
                                        sizeof(eeprom_test_backup)) == STORAGE_OK) &&
                         (storage_sync(storage) == STORAGE_OK) &&
                         (storage_read(storage,
                                       EEPROM_TEST_ADDRESS,
                                       eeprom_test_read_data,
                                       sizeof(eeprom_test_read_data)) == STORAGE_OK) &&
                         (memcmp(eeprom_test_backup,
                                 eeprom_test_read_data,
                                 sizeof(eeprom_test_backup)) == 0);

    return result->backup_ok && result->program_ok && result->verify_ok &&
           result->bounds_ok && result->restore_ok;
}
#endif

void app_main_init(void)
{
    const char *reset_reason;
    const spi_nor_bus_t flash_bus = {
        .context = NULL,
        .transfer = app_flash_transfer,
        .chip_select = app_flash_chip_select,
        .delay = app_flash_delay,
    };
    const eeprom_24c02_bus_t eeprom_bus = {
        .context = NULL,
        .is_ready = app_eeprom_is_ready,
        .read = app_eeprom_read,
        .write = app_eeprom_write,
    };
    spi_nor_jedec_id_t flash_id = {0};
    const spi_nor_device_info_t *flash_info = NULL;
    struct storage_info flash_storage_info = {0};
    struct storage_info eeprom_storage_info = {0};
#if APP_ENABLE_SPI_NOR_SELF_TEST
    app_flash_test_result_t flash_test_result = {0};
#endif
#if APP_ENABLE_EEPROM_SELF_TEST
    app_eeprom_test_result_t eeprom_test_result = {0};
#endif
    bool flash_read_ok;
    bool eeprom_probe_ok;
    bool flash_storage_ok = false;
    bool flash_partitions_ok = false;
    uint32_t default_stack_free_words;
    uint32_t serial_stack_free_words;
    serial_manager_stats_t serial_stats = {0};
    size_t heap_free_bytes;
    size_t heap_min_free_bytes;
    bool heap_unused;
    bool rtos_health_ok;
#if APP_ENABLE_SPI_NOR_SELF_TEST
    bool flash_rw_test_ok = false;
#endif
#if APP_ENABLE_EEPROM_SELF_TEST
    bool eeprom_rw_test_ok = false;
#endif

    bsp_board_init();
    reset_reason = bsp_board_get_reset_reason();
    flash_read_ok = spi_nor_init(&app_flash_driver, &flash_bus) &&
                    spi_nor_release_power_down(&app_flash_driver);
    HAL_Delay(1U);
    flash_read_ok = flash_read_ok && spi_nor_reset(&app_flash_driver);
    HAL_Delay(1U);
    flash_read_ok = flash_read_ok &&
                    spi_nor_read_jedec_id(&app_flash_driver, &flash_id);
    if (flash_read_ok)
    {
        flash_info = spi_nor_find_device(&flash_id);
        flash_storage_ok = (flash_info != NULL) &&
                           (storage_spi_nor_bind(&app_flash_storage,
                                                 "spi_nor0",
                                                 &app_flash_adapter,
                                                 &app_flash_driver,
                                                 SPI_NOR_PROGRAM_TIMEOUT_MS,
                                                 SPI_NOR_ERASE_TIMEOUT_MS) == STORAGE_OK) &&
                           (storage_open(&app_flash_storage) == STORAGE_OK) &&
                           (storage_get_info(&app_flash_storage,
                                             &flash_storage_info) == STORAGE_OK);
        if (flash_storage_ok)
        {
            flash_partitions_ok = app_flash_partitions_init();
            if (flash_partitions_ok)
            {
                flash_partitions_ok =
                    update_session_bind_candidate(&app_candidate_storage);
            }
        }
#if APP_ENABLE_SPI_NOR_SELF_TEST
        if (flash_partitions_ok)
        {
            flash_rw_test_ok = app_flash_run_rw_test(&app_driver_test_storage,
                                                      &app_flash_driver,
                                                      &flash_test_result);
        }
#endif
    }
    eeprom_probe_ok =
        eeprom_24c02_init(&app_eeprom_driver, &eeprom_bus) &&
        (storage_eeprom_24c02_bind(&app_eeprom_storage,
                                    "eeprom0",
                                    &app_eeprom_adapter,
                                    &app_eeprom_driver,
                                    EEPROM_TRANSFER_TIMEOUT_MS) == STORAGE_OK) &&
        (storage_open(&app_eeprom_storage) == STORAGE_OK) &&
        (storage_get_info(&app_eeprom_storage,
                          &eeprom_storage_info) == STORAGE_OK);
#if APP_ENABLE_EEPROM_SELF_TEST
    if (eeprom_probe_ok)
    {
        eeprom_rw_test_ok = app_eeprom_run_rw_test(&app_eeprom_storage,
                                                    &app_eeprom_driver,
                                                    &eeprom_test_result);
    }
#endif

    (void)app_log_printf("\r\n================================\r\n");
    (void)app_log_printf("STM32F407 Application\r\n");
    (void)app_log_printf("Version       : %s\r\n", APPLICATION_VERSION);
    (void)app_log_printf("Build         : %s %s\r\n", __DATE__, __TIME__);
    (void)app_log_printf("Reset reason  : %s\r\n", reset_reason);
    (void)app_log_printf("SYSCLK        : %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    (void)app_log_printf("HCLK          : %lu Hz\r\n", (unsigned long)HAL_RCC_GetHCLKFreq());
    (void)app_log_printf("PCLK1         : %lu Hz\r\n", (unsigned long)HAL_RCC_GetPCLK1Freq());
    (void)app_log_printf("PCLK2         : %lu Hz\r\n", (unsigned long)HAL_RCC_GetPCLK2Freq());
    (void)app_log_printf("SystemCoreClk : %lu Hz\r\n", (unsigned long)SystemCoreClock);
    (void)app_log_printf("Vector table  : 0x%08lX\r\n", (unsigned long)SCB->VTOR);
    (void)app_log_printf("HAL timebase  : TIM7\r\n");
    (void)app_log_printf("RTOS tick     : %lu Hz\r\n", (unsigned long)configTICK_RATE_HZ);
    (void)app_log_printf("USART1        : 115200 8N1\r\n");
    (void)app_log_printf("SPI1          : Mode 0, 5250000 Hz\r\n");
    (void)app_log_printf("I2C1          : 100000 Hz\r\n");
    (void)app_log_printf("Storage spi_nor0: %s, %lu bytes, erase %lu\r\n",
                         flash_storage_ok ? "OPEN" : "FAILED",
                         (unsigned long)flash_storage_info.capacity_bytes,
                         (unsigned long)flash_storage_info.erase_size_bytes);
    (void)app_log_printf("Storage eeprom0 : %s, %lu bytes, no erase\r\n",
                         eeprom_probe_ok ? "OPEN" : "FAILED",
                         (unsigned long)eeprom_storage_info.capacity_bytes);
    (void)app_log_printf("Storage partitions: %s\r\n",
                         flash_partitions_ok ? "READY" : "FAILED");
    if (flash_read_ok)
    {
        (void)app_log_printf("SPI NOR JEDEC : %02X %02X %02X\r\n",
                             flash_id.manufacturer,
                             flash_id.memory_type,
                             flash_id.capacity);
        (void)app_log_printf("SPI NOR model : %s\r\n",
                             (flash_info != NULL) ? flash_info->model : "UNKNOWN");
        (void)app_log_printf("SPI NOR check : %s\r\n",
                             (flash_info != NULL) ? "SUPPORTED" : "UNSUPPORTED");
    }
    else
    {
        (void)app_log_printf("SPI NOR JEDEC : READ ERROR\r\n");
        (void)app_log_printf("SPI NOR model : UNKNOWN\r\n");
        (void)app_log_printf("SPI NOR check : FAILED\r\n");
    }
#if APP_ENABLE_SPI_NOR_SELF_TEST
    (void)app_log_printf("SPI NOR self-test: ENABLED\r\n");
    (void)app_log_printf("SPI NOR test   : 0x%08lX, %u bytes\r\n",
                         (unsigned long)APP_STORAGE_DRIVER_TEST_OFFSET,
                         (unsigned int)SPI_NOR_TEST_LENGTH);
    (void)app_log_printf("SPI NOR retained: %s\r\n",
                         flash_test_result.retained_state_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR erase  : %s\r\n", flash_test_result.erase_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR program: %s\r\n", flash_test_result.program_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR verify : %s\r\n", flash_test_result.verify_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR cross-page: %s\r\n",
                         flash_test_result.cross_page_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR flash-end : %s\r\n",
                         flash_test_result.flash_end_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR bounds    : %s\r\n",
                         flash_test_result.bounds_ok ? "PASS" : "FAIL");
    if (!flash_test_result.bounds_ok)
    {
        (void)app_log_printf("SPI NOR bounds diag: page=%u read=%d write=%d erase=%d\r\n",
                             flash_test_result.page_boundary_rejected ? 1U : 0U,
                             flash_test_result.read_range_status,
                             flash_test_result.write_range_status,
                             flash_test_result.erase_alignment_status);
    }
    (void)app_log_printf("SPI NOR cleanup: %s\r\n", flash_test_result.cleanup_ok ? "PASS" : "FAIL");
    (void)app_log_printf("SPI NOR R/W test: %s\r\n", flash_rw_test_ok ? "PASS" : "FAIL");
#else
    (void)app_log_printf("SPI NOR self-test: DISABLED\r\n");
#endif
    (void)app_log_printf("EEPROM address: 0x%02X (7-bit)\r\n",
                         EEPROM_24C02_ADDRESS_7BIT);
    (void)app_log_printf("EEPROM probe  : %s\r\n",
                         eeprom_probe_ok ? "PASS" : "FAIL");
#if APP_ENABLE_EEPROM_SELF_TEST
    (void)app_log_printf("EEPROM self-test: ENABLED\r\n");
    (void)app_log_printf("EEPROM test   : 0x%02X, %u bytes\r\n",
                         EEPROM_TEST_ADDRESS,
                         EEPROM_TEST_LENGTH);
    (void)app_log_printf("EEPROM backup : %s\r\n",
                         eeprom_test_result.backup_ok ? "PASS" : "FAIL");
    (void)app_log_printf("EEPROM program: %s\r\n",
                         eeprom_test_result.program_ok ? "PASS" : "FAIL");
    (void)app_log_printf("EEPROM verify : %s\r\n",
                         eeprom_test_result.verify_ok ? "PASS" : "FAIL");
    (void)app_log_printf("EEPROM bounds : %s\r\n",
                         eeprom_test_result.bounds_ok ? "PASS" : "FAIL");
    (void)app_log_printf("EEPROM restore: %s\r\n",
                         eeprom_test_result.restore_ok ? "PASS" : "FAIL");
    (void)app_log_printf("EEPROM R/W test: %s\r\n",
                         eeprom_rw_test_ok ? "PASS" : "FAIL");
#else
    (void)app_log_printf("EEPROM self-test: DISABLED\r\n");
#endif

    default_stack_free_words =
        (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    serial_stack_free_words =
        serial_manager_get_stack_high_water_mark_words();
    serial_manager_get_stats(&serial_stats);
    heap_free_bytes = xPortGetFreeHeapSize();
    heap_min_free_bytes = xPortGetMinimumEverFreeHeapSize();
    /* heap_4 reports 0/0 until its first allocation. P1 requires all
       application RTOS objects to remain statically allocated. */
    heap_unused = (heap_free_bytes == 0U) && (heap_min_free_bytes == 0U);
    rtos_health_ok =
        (default_stack_free_words >= APP_RTOS_MIN_STACK_FREE_WORDS) &&
        (serial_stack_free_words >= APP_RTOS_MIN_STACK_FREE_WORDS) &&
        heap_unused;

    (void)app_log_printf("RTOS objects   : STATIC\r\n");
    (void)app_log_printf("RTOS default stack free: %lu words\r\n",
                         (unsigned long)default_stack_free_words);
    (void)app_log_printf("RTOS serial stack free : %lu words\r\n",
                         (unsigned long)serial_stack_free_words);
    (void)app_log_printf("RTOS heap free/min     : %lu/%lu bytes\r\n",
                         (unsigned long)heap_free_bytes,
                         (unsigned long)heap_min_free_bytes);
    (void)app_log_printf("RTOS heap state        : %s\r\n",
                         heap_unused ? "UNUSED" : "TOUCHED");
    (void)app_log_printf("RTOS health    : %s\r\n",
                         rtos_health_ok ? "PASS" : "FAIL");
    (void)app_log_printf("APP log drops  : %lu\r\n",
                         (unsigned long)app_log_get_dropped_count());
    (void)app_log_printf("Serial mode    : CONSOLE\r\n");
    (void)app_log_printf("Serial RX/drop : %lu/%lu bytes\r\n",
                         (unsigned long)serial_stats.rx_bytes,
                         (unsigned long)serial_stats.rx_dropped);
    (void)app_log_printf("Serial HW overrun/error: %lu/%lu\r\n",
                         (unsigned long)serial_stats.rx_hardware_overruns,
                         (unsigned long)serial_stats.rx_hardware_errors);
    (void)app_log_printf("APP address   : 0x%08lX\r\n", (unsigned long)APPLICATION_FLASH_BASE);
    (void)app_log_printf("APP state     : DEVELOPMENT\r\n");
    (void)app_log_printf("================================\r\n");

    bsp_board_clear_reset_flags();
}

void app_main_task(void *argument)
{
    uint32_t period_ms;
    uint32_t on_time_ms;

    (void)argument;

    for (;;)
    {
        if (system_mode_get() != SYSTEM_MODE_NORMAL)
        {
            BSP_LED1_OFF();
            BSP_LED2_OFF();
            (void)system_mode_wait_normal(portMAX_DELAY);
            continue;
        }
        period_ms = app_main_get_heartbeat_ms();
        on_time_ms = (period_ms < 200U) ? (period_ms / 2U) : 100U;
        BSP_LED1_ON();
        BSP_LED2_ON();
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));

        BSP_LED1_OFF();
        BSP_LED2_OFF();
        vTaskDelay(pdMS_TO_TICKS(period_ms - on_time_ms));
    }
}

bool app_main_set_heartbeat_ms(uint32_t period_ms)
{
    if ((period_ms < APP_HEARTBEAT_MIN_MS) ||
        (period_ms > APP_HEARTBEAT_MAX_MS))
    {
        return false;
    }
    taskENTER_CRITICAL();
    app_heartbeat_ms = period_ms;
    taskEXIT_CRITICAL();
    return true;
}

uint32_t app_main_get_heartbeat_ms(void)
{
    uint32_t period_ms;

    taskENTER_CRITICAL();
    period_ms = app_heartbeat_ms;
    taskEXIT_CRITICAL();
    return period_ms;
}
