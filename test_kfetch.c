#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << 6) - 1)

int main(int argc, char *argv[]) {
    int fd, mask;
    char buffer[4096];
    
    fd = open("/dev/kfetch", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }
    
    // Set mask based on command-line arg or use default
    if (argc > 1) {
        mask = atoi(argv[1]);
    } else {
        // Default: Show CPU model and memory info
        mask = KFETCH_FULL_INFO;
    }
    
    // Set the mask
    write(fd, &mask, sizeof(mask));
    
    // Read the info
    read(fd, buffer, sizeof(buffer));
    
    printf("%s\n", buffer);
    
    close(fd);
    return 0;
}
