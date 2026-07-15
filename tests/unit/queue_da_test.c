/*
 * Unit coverage for disk-assisted engine selection and its durable marker.
 * Each case builds an exact temporary spool layout; the oracle is the chosen
 * engine or an explicit rejection, so no daemon timing is involved.
 */
#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "queue_da.h"

/* Keep this internal helper unit-testable without linking the queue runtime. */
#include "../../runtime/queue_da.c"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

static void touch_file(const char *dir, const char *name, const char *contents) {
    char path[512];
    CHECK(snprintf(path, sizeof(path), "%s/%s", dir, name) < (int)sizeof(path));
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    CHECK(fd >= 0);
    if (contents != NULL) CHECK(write(fd, contents, strlen(contents)) == (ssize_t)strlen(contents));
    CHECK(close(fd) == 0);
}

static void check_file_contents(const char *dir, const char *name, const char *expected) {
    char path[512];
    char contents[128];
    CHECK(snprintf(path, sizeof(path), "%s/%s", dir, name) < (int)sizeof(path));
    const int fd = open(path, O_RDONLY);
    CHECK(fd >= 0);
    const ssize_t len = read(fd, contents, sizeof(contents) - 1);
    CHECK(len >= 0);
    contents[len] = '\0';
    CHECK(close(fd) == 0);
    CHECK(strcmp(contents, expected) == 0);
}

static qda_engine_result_t resolve(
    const char *dir, qda_engine_mode_t requested, sbool auto_upgrade, sbool classic_features, rsRetVal expected) {
    const qda_engine_config_t config = {
        .spool_dir = dir,
        .file_prefix = "queue",
        .requested = requested,
        .auto_upgrade = auto_upgrade,
        .requires_classic_features = classic_features,
    };
    qda_engine_result_t result;
    /* Callers may ignore the returned result for rejection cases because this
     * helper itself makes the return-code expectation a fatal test oracle. */
    CHECK(qdaEngineResolve(&config, &result) == expected);
    return result;
}

int main(void) {
    char root[] = "/tmp/rsyslog-qda-unit-XXXXXX";
    CHECK(mkdtemp(root) != NULL);

    const qda_lifecycle_config_t lifecycle = {
        .engine = QDA_ENGINE_AUTO,
        .auto_upgrade = 0,
        .idle_timeout = 60000,
    };
    qda_lifecycle_config_t changed = lifecycle;
    CHECK(qdaLifecycleConfigEqual(&lifecycle, &changed));
    changed.engine = QDA_ENGINE_DISK;
    CHECK(!qdaLifecycleConfigEqual(&lifecycle, &changed));
    changed = lifecycle;
    changed.auto_upgrade = 1;
    CHECK(!qdaLifecycleConfigEqual(&lifecycle, &changed));
    changed = lifecycle;
    changed.idle_timeout = -1;
    CHECK(!qdaLifecycleConfigEqual(&lifecycle, &changed));
    CHECK(!qdaLifecycleConfigEqual(NULL, &changed));

    qda_engine_result_t result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK);
    CHECK(result.reason == QDA_REASON_FRESH_DEFAULT);

    result = resolve(root, QDA_ENGINE_AUTO, 0, 1, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK);
    CHECK(result.reason == QDA_REASON_CLASSIC_FEATURE);
    resolve(root, QDA_ENGINE_SEGMENTED_DISK, 0, 1, RS_RET_INVALID_VALUE);

    touch_file(root, "queue.qi", "classic");
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK && result.classic_data);
    CHECK(resolve(root, QDA_ENGINE_SEGMENTED_DISK, 0, 0, RS_RET_INVALID_VALUE).classic_data);
    char path[512];
    CHECK(snprintf(path, sizeof(path), "%s/queue.qi", root) < (int)sizeof(path));
    CHECK(unlink(path) == 0);

    touch_file(root, "queue.0000001", "classic segment");
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK && result.classic_data);
    CHECK(snprintf(path, sizeof(path), "%s/queue.0000001", root) < (int)sizeof(path));
    CHECK(unlink(path) == 0);

    CHECK(qdaEngineWriteMarker(root, "queue", QDA_ENGINE_DISK) == RS_RET_OK);
    CHECK(qdaEngineWriteMarker(root, "queue", QDA_ENGINE_AUTO) == RS_RET_PARAM_ERROR);
    CHECK(qdaEngineWriteMarker(root, "queue", (qda_engine_mode_t)99) == RS_RET_PARAM_ERROR);
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK && result.reason == QDA_REASON_MARKER);
    /* A marker without state or segments records an empty former engine, not
     * a backlog. An explicit selector may therefore replace it for the next
     * materialization; the data-conflict cases above and below must reject. */
    result = resolve(root, QDA_ENGINE_SEGMENTED_DISK, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK && result.reason == QDA_REASON_CONFIGURED);
    result = resolve(root, QDA_ENGINE_AUTO, 1, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK && result.reason == QDA_REASON_AUTO_UPGRADE);

    /* Model a crash-left temporary marker from this process. The atomic
     * replacement helper must remove it, durably replace the live marker, and
     * leave no temporary name that could confuse a later restart. */
    char tmp_name[128];
    CHECK(snprintf(tmp_name, sizeof(tmp_name), "queue.da-engine.tmp.%ld", (long)getpid()) < (int)sizeof(tmp_name));
    touch_file(root, tmp_name, "incomplete marker");
    CHECK(qdaEngineWriteMarker(root, "queue", QDA_ENGINE_SEGMENTED_DISK) == RS_RET_OK);
    check_file_contents(root, "queue.da-engine", "RSYSLOG-DA-ENGINE-V1 segmentedDisk\n");
    CHECK(snprintf(path, sizeof(path), "%s/%s", root, tmp_name) < (int)sizeof(path));
    CHECK(access(path, F_OK) != 0);
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK && result.marker_present);
    result = resolve(root, QDA_ENGINE_AUTO, 0, 1, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK && result.reason == QDA_REASON_CLASSIC_FEATURE);
    result = resolve(root, QDA_ENGINE_DISK, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK && result.reason == QDA_REASON_CONFIGURED);

    CHECK(snprintf(path, sizeof(path), "%s/queue.segq", root) < (int)sizeof(path));
    CHECK(mkdir(path, 0700) == 0);
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK && result.segmented_data);
    resolve(root, QDA_ENGINE_DISK, 0, 0, RS_RET_INVALID_VALUE);
    touch_file(root, "queue.1", "classic segment");
    resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_INVALID_VALUE);
    CHECK(snprintf(path, sizeof(path), "%s/queue.1", root) < (int)sizeof(path));
    CHECK(unlink(path) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/queue.segq", root) < (int)sizeof(path));
    CHECK(rmdir(path) == 0);

    touch_file(root, "queue.da-engine", "bad marker\n");
    resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_INVALID_VALUE);
    CHECK(snprintf(path, sizeof(path), "%s/queue.da-engine", root) < (int)sizeof(path));
    CHECK(unlink(path) == 0);
    CHECK(rmdir(root) == 0);

    puts("disk-assisted engine resolver tests passed");
    return 0;
}
