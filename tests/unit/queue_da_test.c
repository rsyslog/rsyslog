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
    CHECK(qdaEngineResolve(&config, &result) == expected);
    return result;
}

int main(void) {
    char root[] = "/tmp/rsyslog-qda-unit-XXXXXX";
    CHECK(mkdtemp(root) != NULL);

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
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_DISK && result.reason == QDA_REASON_MARKER);
    result = resolve(root, QDA_ENGINE_AUTO, 1, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK && result.reason == QDA_REASON_AUTO_UPGRADE);
    CHECK(qdaEngineWriteMarker(root, "queue", QDA_ENGINE_SEGMENTED_DISK) == RS_RET_OK);
    result = resolve(root, QDA_ENGINE_AUTO, 0, 0, RS_RET_OK);
    CHECK(result.effective == QDA_ENGINE_SEGMENTED_DISK && result.marker_present);

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
