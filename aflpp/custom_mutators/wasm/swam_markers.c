#include "swam_markers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SWAM_MARKERS_TMP_PATH "/tmp/swam_markers.txt"
#define SWAM_MARKERS_ENV "SWAM_SHM_ID"
#define SWAM_MARKERS_WORDS 3U

static int s_shmid = -1;
static uint32_t* s_shm = NULL;

static const char* get_markers_file_path(void) {

    const char* env = getenv("SWAM_MARKERS_FILE");
    return (env && env[0]) ? env : SWAM_MARKERS_TMP_PATH;

}

static void swam_markers_write_defaults(uint32_t* before, uint32_t* after, bool* valid) {

    if (before != NULL) *before = 0;
    if (after != NULL) *after = 0;
    if (valid != NULL) *valid = false;

}

int swam_markers_init(void) {

    char id_buf[32];
    int id_len = 0;
    void* shm_ptr = NULL;

    if (s_shm != NULL || s_shmid != -1) {
        swam_markers_cleanup();
    }

    s_shmid = shmget(IPC_PRIVATE, SWAM_MARKERS_WORDS * sizeof(uint32_t), IPC_CREAT | 0600);
    if (s_shmid == -1) return -1;

    shm_ptr = shmat(s_shmid, NULL, 0);
    if (shm_ptr == (void*)-1) {
        shmctl(s_shmid, IPC_RMID, NULL);
        s_shmid = -1;
        return -1;
    }

    s_shm = (uint32_t*)shm_ptr;
    memset(s_shm, 0, SWAM_MARKERS_WORDS * sizeof(uint32_t));

    id_len = snprintf(id_buf, sizeof(id_buf), "%d", s_shmid);
    if (id_len < 0 || (size_t)id_len >= sizeof(id_buf) ||
        setenv(SWAM_MARKERS_ENV, id_buf, 1) != 0) {
        shmdt(s_shm);
        s_shm = NULL;
        shmctl(s_shmid, IPC_RMID, NULL);
        s_shmid = -1;
        return -1;
    }

    return 0;

}

void swam_markers_read(uint32_t* before, uint32_t* after, bool* valid) {

    FILE* marker_file = NULL;
    unsigned int file_before = 0;
    unsigned int file_after = 0;

    swam_markers_write_defaults(before, after, valid);

    if (s_shm != NULL && s_shm[2] != 0U) {
        if (before != NULL) *before = s_shm[0];
        if (after != NULL) *after = s_shm[1];
        if (valid != NULL) *valid = true;
        return;
    }

    marker_file = fopen(get_markers_file_path(), "r");
    if (marker_file == NULL) return;

    if (fscanf(marker_file, "%u %u", &file_before, &file_after) == 2) {
        if (before != NULL) *before = (uint32_t)file_before;
        if (after != NULL) *after = (uint32_t)file_after;
        if (valid != NULL) *valid = true;
    }

    fclose(marker_file);

}

void swam_markers_reset(void) {

    if (s_shm != NULL) {
        memset(s_shm, 0, SWAM_MARKERS_WORDS * sizeof(uint32_t));
    }

    unlink(get_markers_file_path());

}

void swam_markers_cleanup(void) {

    if (s_shm != NULL) {
        shmdt(s_shm);
        s_shm = NULL;
    }

    if (s_shmid != -1) {
        shmctl(s_shmid, IPC_RMID, NULL);
        s_shmid = -1;
    }

    unlink(get_markers_file_path());
    unsetenv(SWAM_MARKERS_ENV);

}
