#include "Core/EntryPoint.h"

#include "Core/Uuid.h"
#include "Core/Clock.h"
#include "Core/HashTableGeneric.h"

const usize GEngineMemoryAmount  = Gigabytes(1);
const usize GEngineScratchAmount = Kibibytes(8);

UNUSED static void StringTableTest(u32 MaxEntries)
{
    HashTable_Stringu32 Table;
    Table.Entries = Array_Reserve(HashTableEntry_Stringu32, DEFAULT_HASHTABLE_CAPACITY);

    usize LastCapacity = Array_Capacity(Table.Entries);
    f64 InsertionTimeSum = 0;
    f64 InsertionTime = 0;
    u32 NumInsertionsBeforeGrowth = 0;

    while (1)
    {
        Uuid ID = UUID_Generate();
        StringLocal(StringID, UUID_STRING_LENGTH);
        UUID_ToStringFast(ID, &StringID);

        String AllocatedString;
        AllocatedString.Data = MemAlloc(UUID_STRING_LENGTH, MemoryTag_Map);
        MemCopy(AllocatedString.Data, StringID.Data, StringID.Length);
        AllocatedString.Length = UUID_STRING_LENGTH;
        AllocatedString.Capacity = UUID_STRING_LENGTH;

        usize NumBeforeInsert = Array_Num(Table.Entries);

        Clock c;
        Clock_Start(&c);
        HashTable_Stringu32_Add(&Table, AllocatedString, ID.TimeLow);
        Clock_Tick(&c);

        InsertionTimeSum += c.ElapsedTime;
        NumInsertionsBeforeGrowth += 1;

        // we grew. log statistics
        if (LastCapacity != Array_Capacity(Table.Entries))
        {
            f64 InsertionTimeAverage = InsertionTimeSum / (f64)NumInsertionsBeforeGrowth;

            StringLocal(TimeAvg, 64);
            Time_ToString(InsertionTimeAverage, true, &TimeAvg);

            StringLocal(Time, 64);
            Time_ToString(InsertionTimeSum, true, &Time);

            StringLocal(LastTime, 64);
            Time_ToString(InsertionTime, true, &LastTime);

            f32 LoadFactor = ((f32)NumBeforeInsert/(f32)LastCapacity) * 100.0f;
            LOG("\n== Hash Table Entries/Size: %u / %u (Load Factor: %.2f%%) ==", Array_Num(Table.Entries), LastCapacity, LoadFactor);
            LOG("Inserts in this time block: %u", NumInsertionsBeforeGrowth);
            LOG("Average Insertion Time: %S", TimeAvg);
            LOG("Last Insertion Time: %S", LastTime);
            LOG("Elapsed Time: %S", Time);

            LastCapacity = Array_Capacity(Table.Entries);

            InsertionTimeSum = 0;
            NumInsertionsBeforeGrowth = 0;

            // we've tested enough now, exit the test loop
            if (LastCapacity > MaxEntries)
            {
                break;
            }
        }
        else
        {
            InsertionTime = c.ElapsedTime;
        }
    }
}

static void u32TableTest(u32 MaxEntries)
{
    HashTable_u32u32 Table;
    Table.Entries = Array_Reserve(HashTableEntry_u32u32, 16384);

    usize LastCapacity = Array_Capacity(Table.Entries);
    f64 InsertionTimeSum = 0;
    f64 InsertionTime = 0;
    u32 NumInsertionsBeforeGrowth = 0;

    u32 i = 0;
    while (1)
    {
        usize NumBeforeInsert = Array_Num(Table.Entries);

        Clock c;
        Clock_Start(&c);
        HashTable_u32u32_Add(&Table, i, i);
        Clock_Tick(&c);

        InsertionTimeSum += c.ElapsedTime;
        NumInsertionsBeforeGrowth += 1;

        // we grew. log statistics
        if (LastCapacity != Array_Capacity(Table.Entries))
        {
            f64 InsertionTimeAverage = InsertionTimeSum / (f64)NumInsertionsBeforeGrowth;

            StringLocal(TimeAvg, 64);
            Time_ToString(InsertionTimeAverage, true, &TimeAvg);

            StringLocal(Time, 64);
            Time_ToString(InsertionTimeSum, true, &Time);

            StringLocal(LastTime, 64);
            Time_ToString(InsertionTime, true, &LastTime);

            f32 LoadFactor = ((f32)NumBeforeInsert/(f32)LastCapacity) * 100.0f;
            LOG("\n== Hash Table Entries/Size: %u / %u (Load Factor: %.2f%%) ==", Array_Num(Table.Entries), LastCapacity, LoadFactor);
            LOG("Inserts in this time block: %u", NumInsertionsBeforeGrowth);
            LOG("Average Insertion Time: %S", TimeAvg);
            LOG("Last Insertion Time: %S", LastTime);
            LOG("Elapsed Time: %S", Time);

            LastCapacity = Array_Capacity(Table.Entries);
            InsertionTimeSum = 0;
            NumInsertionsBeforeGrowth = 0;

            // we've tested enough now, exit the test loop
            if (LastCapacity > MaxEntries)
            {
                break;
            }
        }
        else
        {
            InsertionTime = c.ElapsedTime;
        }

        i++;
    }
}

u32 RunApplication(const StringArray Arguments)
{
    Logging_ToggleLogFile(false);
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);

    LOG("== String Hash Table Tests ==\n");
    StringTableTest(200000);

    LOG("\n== u32 Hash Table Tests ==\n");
    u32TableTest(200000);

    LOG("\n== Done ==");

    return 0;
}

