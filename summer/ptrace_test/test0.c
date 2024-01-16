#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include<dlfcn.h>
#include<sys/user.h>
#include <sys/mman.h>

void print_bytes(unsigned long data) {
    for (int i = 0; i < sizeof(unsigned long); i++) {
        unsigned char byte = (data >> (i * 8)) & 0xff;
        printf("%02x ", byte);
    }
    printf("\n");
}

int main() {
    pid_t child;
    long data;
    void* lib_address;
    void* func_address;

    struct user_regs_struct regs;

    child = fork();
    if (child == 0) {
        // 子进程
        printf("Child process: Hello from child!\n");
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl("./noF_debug", "noF_debug", NULL);
    } else {
        // 父进程
        printf("Parent process: Child process has been started.\n");
        // waitpid(child,0,0);
        wait(NULL);
        // 附加到子进程
        if (ptrace(PTRACE_ATTACH, child, NULL, NULL) == -1) {
            perror("ptrace attach");
            // return 1;
        }
        
        ptrace(PTRACE_GETREGS, child ,NULL, &regs);
        // 输出断点触发位置
        printf("address: %llx\n", regs.rip);
        // wait(NULL);
        printf("Child process has been started.\n");
        ptrace(PTRACE_GETREGS, child ,NULL, &regs);
        // 输出断点触发位置
        printf("address: %llx\n", regs.rip);
        ptrace(PTRACE_GETREGS, child ,NULL, &regs);
        // 输出断点触发位置
        printf("address: %llx\n", regs.rip);

        //open lib4.so
        void* handle = dlopen("/home/cyn/Desktop/DA/summer/ptrace_test/lib4.so", RTLD_LAZY);
        if (handle == NULL) {
            fprintf(stderr, "Failed to open library: %s\n", dlerror());
            return 1;
        }
        // 获取共享库中的函数地址
        void (*func_start)() = dlsym(handle, "foo");
        if (func_start == NULL) {
            fprintf(stderr, "Failed to get function pointer: %s\n", dlerror());
            dlclose(handle);
            return 1;
        }
        printf("Start address of 'foo' function: %p\n", (void*)func_start);
        long func_size = 23;
        printf("End address of 'foo' function : %p\n",(void *)(func_start + func_size));
        

       

        void* local_mem = malloc(func_size);
        memcpy(local_mem, func_start, func_size);
        printf("Code of function foo:\n");
        for (size_t i = 0; i < func_size; i++) {
            printf("%02x ", ((unsigned char*)local_mem)[i]);
        }
        printf("\n");

        //locate the GOT
        char maps_path[256];
        sprintf(maps_path, "/proc/%d/maps", child);
        FILE* maps_file = fopen(maps_path, "r");
        unsigned long function_address,main,G;
        if (maps_file != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), maps_file)) {
                if (strstr(line, "noF_debug") != NULL) {
                    // 找到可执行文件所在的行
                    unsigned long start, end;
                    sscanf(line, "%lx-%lx", &start, &end);
                    printf("Addr of start:%lx\n", start);
                    unsigned long function_offset = 0x3fc8;  // 静态文件中的地址偏移量
                    function_address = start + function_offset;
                    G = start + 0x11a9;
                    main = start + 0x11c5;
                    break;
                }
            }
            fclose(maps_file);
        }
        printf("Addr of function F1'GOT:%lx\n", function_address);
        printf("Addr of function main:%lx\n", main);


        ptrace(PTRACE_GETREGS, child ,NULL, &regs);
        // 输出断点触发位置
        printf("address: %llx\n", regs.rip);
        // 保存原始指令数据
        long orig_data = ptrace(PTRACE_PEEKTEXT, child, (void *)main, NULL);

        printf("original code in main : %lx\n",orig_data);
        // 设置断点指令
        data = (orig_data & ~0xFF) | 0xCC;
        ptrace(PTRACE_POKETEXT, child, (void *)main, (void *)data);

        // 恢复子进程运行
        ptrace(PTRACE_CONT, child, NULL, NULL);

        // 等待子进程再次停止，表示断点触发
        wait(NULL);

        ptrace(PTRACE_GETREGS, child ,NULL, &regs);
        // 输出断点触发位置
        printf("Breakpoint hit at address: %llx\n", regs.rip);

         // 恢复原始指令
        ptrace(PTRACE_POKETEXT, child, (void *)(regs.rip-1), (void *)orig_data);

        
        // Allocate memory in child process
        void* remote_addr = (void*)regs.rsp - func_size;
        
        // Write the code to child process memory
        // for (size_t i = 0; i < func_size / sizeof(long); i++) {
        //     long data = *((long*)((char*)local_mem + i * sizeof(long)));
        //     ptrace(PTRACE_POKEDATA, child, (void*)((char*)remote_addr + i * sizeof(long)), (void*)data);
        // }

        // Calculate the number of long-sized blocks
        size_t num_blocks = func_size / sizeof(long);

        // Write the code to child process memory (integer-sized blocks)
        for (size_t i = 0; i < num_blocks; i++) {
            long data = *((long*)((char*)local_mem + i * sizeof(long)));
            ptrace(PTRACE_POKEDATA, child, (void*)((char*)remote_addr + i * sizeof(long)), (void*)data);
        }

        // Write the remaining bytes (if any)
        size_t remaining_bytes = func_size % sizeof(long);
        if (remaining_bytes > 0) {
            long last_data = 0;
            memcpy(&last_data, (char*)local_mem + num_blocks * sizeof(long), remaining_bytes);
            ptrace(PTRACE_POKEDATA, child, (void*)((char*)remote_addr + num_blocks * sizeof(long)), (void*)last_data);
        }


        // Read the code from child process memory
        for (size_t i = 0; i < func_size / sizeof(long)+1; i++) {
            long data = ptrace(PTRACE_PEEKDATA, child, (void*)((char*)remote_addr + i * sizeof(long)), NULL);
            printf("%lx\n", data);
        }
        printf("%p\n",remote_addr);


        ptrace(PTRACE_POKETEXT, child, (void *)function_address,(void *)(G));

        data = ptrace(PTRACE_PEEKDATA, child, (void*)function_address, NULL);
        printf("Context of address %lx is: %lx\n", function_address,data);


        // 继续执行子进程
        ptrace(PTRACE_CONT, child, NULL, NULL);
        wait(NULL);
        printf("Child process has exited.\n");
        dlclose(handle);  // 关闭共享库
    }

    
    return 0;
}
