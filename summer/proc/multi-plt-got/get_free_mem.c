#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
// 数据结构表示内存映射区域
typedef struct MemoryRegion {
    unsigned long start;
    unsigned long end;
    struct MemoryRegion* next;
} MemoryRegion;

// 函数用于解析 /proc/pid/maps 文件，并返回一个链表，表示空闲区域
MemoryRegion* find_free_memory_regions(pid_t pid) {
    char filename[20];
    sprintf(filename, "/proc/%d/maps", pid);

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    MemoryRegion* head = NULL;
    MemoryRegion* current = NULL;

    unsigned long prev_end = 0;

    // 读取文件内容并解析
    while (1) {
        unsigned long start, end;
        if (fscanf(file, "%lx-%lx", &start, &end) != 2) {
            break;  // 读取失败或到达文件末尾
        }

        char line[256];
        fgets(line, sizeof(line), file);  // 读取整行内容，但不使用

        // 计算空闲区域
        if (prev_end < start && prev_end != 0) {
            MemoryRegion* free_region = malloc(sizeof(MemoryRegion));
            free_region->start = prev_end;
            free_region->end = start;
            free_region->next = NULL;

            // 添加到链表
            if (current != NULL) {
                current->next = free_region;
            } else {
                head = free_region;
            }

            current = free_region;
        }

        prev_end = end;
    }

    fclose(file);
    return head;
}

// 函数释放链表内存
void free_memory_regions(MemoryRegion* head) {
    while (head != NULL) {
        MemoryRegion* temp = head;
        head = head->next;
        free(temp);
    }
}

// 函数打印链表内容
void print_memory_regions(MemoryRegion* head) {
    while (head != NULL) {
        printf("Start: %lx, End: %lx\n", head->start, head->end);
        head = head->next;
    }
}

// 寻找空闲区域中与指定地址相差不超过32位且为0x1000的倍数的最小地址
// 函数找到空闲区域中最小的地址，差异在32位带符号整数范围内且是0x1000的倍数
unsigned long find_min_address(MemoryRegion* head, unsigned long target_addr) {
    unsigned long min_address = 0;
    long min_diff = 0x7FFFFFFF;  // 初始设置为较大的正数

    while (head != NULL) {
        // 检查目标地址是否在当前节点的范围内
        if (target_addr >= head->start && target_addr <= head->end) {
            return target_addr;  // 如果是，则直接返回目标地址
        }

        // 计算当前节点的起始地址与目标地址的差值
        long start_diff = (long)(head->start - target_addr);
        long end_diff = (long)(head->end - target_addr);

        // 如果差值在32位带符号整数范围内且是0x1000的倍数
        if ((start_diff >= -(1 << 31) && start_diff <= 0x7FFFFFFF && start_diff % 0x1000 == 0) ||
            (end_diff >= -(1 << 31) && end_diff <= 0x7FFFFFFF && end_diff % 0x1000 == 0)) {
            // 在当前节点范围内找到离目标地址最近的0x1000的倍数的地址
            unsigned long current_min_address = head->start - (start_diff % 0x1000);

            // 如果该地址更接近目标地址，更新最小差值和最小地址
            if (current_min_address < min_address) {
                min_address = current_min_address;
                min_diff = start_diff;
            }
        }

        head = head->next;
    }

    return min_address;
}


int main() {
    pid_t pid = 26381;
    MemoryRegion* free_regions = find_free_memory_regions(pid);

    if (free_regions != NULL) {
        printf("Free Memory Regions:\n");
        print_memory_regions(free_regions);

        // 释放链表内存
        free_memory_regions(free_regions);
    }
    printf("%d,%d",(int)0x80000000,0x7fffffff);
    return 0;
}
