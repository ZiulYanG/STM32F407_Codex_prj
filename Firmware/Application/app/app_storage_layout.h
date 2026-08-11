#ifndef APP_STORAGE_LAYOUT_H
#define APP_STORAGE_LAYOUT_H

#define APP_STORAGE_CANDIDATE_OFFSET      0x000000UL
#define APP_STORAGE_CANDIDATE_SIZE        0x100000UL
#define APP_STORAGE_GOLDEN_OFFSET         0x100000UL
#define APP_STORAGE_GOLDEN_SIZE           0x100000UL
#define APP_STORAGE_METADATA_A_OFFSET     0x200000UL
#define APP_STORAGE_METADATA_A_SIZE       0x010000UL
#define APP_STORAGE_METADATA_B_OFFSET     0x210000UL
#define APP_STORAGE_METADATA_B_SIZE       0x010000UL
#define APP_STORAGE_DRIVER_TEST_OFFSET    0xFFF000UL
#define APP_STORAGE_DRIVER_TEST_SIZE      0x001000UL
#define APP_STORAGE_FLASH_CAPACITY        0x1000000UL

_Static_assert(APP_STORAGE_CANDIDATE_OFFSET + APP_STORAGE_CANDIDATE_SIZE <=
                   APP_STORAGE_GOLDEN_OFFSET,
               "Candidate and Golden partitions overlap");
_Static_assert(APP_STORAGE_GOLDEN_OFFSET + APP_STORAGE_GOLDEN_SIZE <=
                   APP_STORAGE_METADATA_A_OFFSET,
               "Golden and Metadata A partitions overlap");
_Static_assert(APP_STORAGE_METADATA_A_OFFSET + APP_STORAGE_METADATA_A_SIZE <=
                   APP_STORAGE_METADATA_B_OFFSET,
               "Metadata partitions overlap");
_Static_assert(APP_STORAGE_METADATA_B_OFFSET + APP_STORAGE_METADATA_B_SIZE <=
                   APP_STORAGE_DRIVER_TEST_OFFSET,
               "Metadata and Driver Test partitions overlap");
_Static_assert(APP_STORAGE_DRIVER_TEST_OFFSET + APP_STORAGE_DRIVER_TEST_SIZE ==
                   APP_STORAGE_FLASH_CAPACITY,
               "Driver Test partition must end at Flash capacity");

#endif /* APP_STORAGE_LAYOUT_H */
