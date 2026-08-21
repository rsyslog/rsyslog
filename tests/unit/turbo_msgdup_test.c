/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Turbo snapshot lifetime and sharing checks on smsg_t.
 *
 * Built only when LOGNORM_TURBO_SUPPORTED. liblognorm itself is not needed:
 * a small fake snapshot stands in for the module-owned blob so the message
 * runtime can be driven directly through the same slots and callbacks
 * mmnormalize uses. Single-threaded sections check the functional contract
 * of every $! access path on a turbo-only message; threaded sections share
 * one message the way action queues (MsgAddRef) and queued rulesets
 * (MsgDup) do, while another thread replaces the snapshot the way a second
 * mmnormalize action does. ASan, LSan and TSan must stay silent and every
 * reader must observe a complete value.
 */
#include "config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsyslog.h"
#include "obj.h"
#include "msg.h"
#include "template.h"
#include "dirty.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            abort();                                                                 \
        }                                                                            \
    } while (0)

#ifndef HAVE_LOGNORM_TURBO
int main(void) {
    return 77; /* automake SKIP */
}
#else

/* The runtime references these rsyslogd.c symbols. No daemon runs here, so
 * they are inert stand-ins; nothing in the test submits or queues. */
rsconf_t *ourConf = NULL;
int iConfigVerify = 0;
int bHaveMainQueue = 0;
int MarkInterval = 20 * 60;

rsRetVal logmsgInternal(const int iErr, const syslog_pri_t pri, const uchar *const msg, int flags) {
    (void)iErr;
    (void)pri;
    (void)flags;
    fprintf(stderr, "rsyslog internal message: %s\n", msg);
    return RS_RET_OK;
}

rsRetVal submitMsg2(smsg_t *pMsg) {
    msgDestruct(&pMsg);
    return RS_RET_OK;
}

rsRetVal multiSubmitMsg2(multi_submit_t *const pMultiSub) {
    (void)pMultiSub;
    return RS_RET_OK;
}

rsRetVal multiSubmitFlush(multi_submit_t *pMultiSub) {
    (void)pMultiSub;
    return RS_RET_OK;
}

rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct nvlst *lst) {
    (void)ppQueue;
    (void)pszQueueName;
    (void)lst;
    return RS_RET_NOT_IMPLEMENTED;
}

rsRetVal startMainQueue(rsconf_t *cnf, qqueue_t *pQueue) {
    (void)cnf;
    (void)pQueue;
    return RS_RET_NOT_IMPLEMENTED;
}

rsRetVal queryLocalHostname(rsconf_t *const pConf) {
    (void)pConf;
    return RS_RET_OK;
}

    /* Built-in module entry points live in tools/ and are only registered when
     * a configuration is loaded, which this test never does. */
    #define INERT_MODINIT(name)                                                                         \
        rsRetVal name(int, int *, rsRetVal (**)(), rsRetVal (*)(uchar *, rsRetVal(**)()), modInfo_t *); \
        rsRetVal name(int iIFVersRequested, int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(),         \
                      rsRetVal (*pHostQueryEtryPt)(uchar *, rsRetVal(**)()), modInfo_t *pModInfo) {     \
            (void)iIFVersRequested;                                                                     \
            (void)ipIFVersProvided;                                                                     \
            (void)pQueryEtryPt;                                                                         \
            (void)pHostQueryEtryPt;                                                                     \
            (void)pModInfo;                                                                             \
            return RS_RET_NOT_IMPLEMENTED;                                                              \
        }
INERT_MODINIT(modInitFile)
INERT_MODINIT(modInitFwd)
INERT_MODINIT(modInitPipe)
INERT_MODINIT(modInitShell)
INERT_MODINIT(modInitUsrMsg)
INERT_MODINIT(modInitDiscard)
INERT_MODINIT(modInitpmrfc3164)
INERT_MODINIT(modInitpmrfc5424)
INERT_MODINIT(modInitsmfile)
INERT_MODINIT(modInitsmfwd)
INERT_MODINIT(modInitsmtradfile)
INERT_MODINIT(modInitsmtradfwd)

    #define FAKE_MAGIC 0x54555242u
    #define NREADERS 4
    #define NDUPPERS 2
    #define DEFAULT_ITERS 20000

struct fake_snap {
    unsigned magic;
    char host[16];
    char tag[16]; /* empty string means "field absent" */
};

static msgPropDescr_t prop_root; /* $!       */
static msgPropDescr_t prop_host; /* $!host   */
static msgPropDescr_t prop_tag; /* $!tag    */
static msgPropDescr_t prop_local; /* $.       */

static void fake_free(void *p) {
    struct fake_snap *s = p;
    CHECK(s != NULL);
    CHECK(s->magic == FAKE_MAGIC);
    s->magic = 0;
    free(s);
}

static void fake_to_json(void *p, struct json_object **out) {
    struct fake_snap *s = p;
    struct json_object *root;

    CHECK(s != NULL && s->magic == FAKE_MAGIC);
    root = json_object_new_object();
    CHECK(root != NULL);
    json_object_object_add(root, "host", json_object_new_string(s->host));
    if (s->tag[0] != '\0') json_object_object_add(root, "tag", json_object_new_string(s->tag));
    *out = root;
}

static int fake_get_str(void *p, const uchar *name, int nlen, const uchar **val, rs_size_t *vlen) {
    struct fake_snap *s = p;
    const char *field = NULL;

    if (s == NULL || s->magic != FAKE_MAGIC) return -1;
    /* names arrive as "!host" after msgPropDescrFill; bare "!" is the tree */
    if (nlen == 5 && memcmp(name, "!host", 5) == 0) field = s->host;
    if (nlen == 4 && memcmp(name, "!tag", 4) == 0 && s->tag[0] != '\0') field = s->tag;
    if (field == NULL) return -1;
    *val = (const uchar *)field;
    *vlen = (rs_size_t)strlen(field);
    return 0;
}

static struct fake_snap *fake_snap_new(const char *host, const char *tag) {
    struct fake_snap *s = malloc(sizeof(*s));
    CHECK(s != NULL);
    s->magic = FAKE_MAGIC;
    CHECK(strlen(host) < sizeof(s->host));
    CHECK(strlen(tag) < sizeof(s->tag));
    memcpy(s->host, host, strlen(host) + 1);
    memcpy(s->tag, tag, strlen(tag) + 1);
    return s;
}

/* Same sequence as the mmnormalize turbo CEE-root path. */
static void attach_snap(smsg_t *msg, const char *host, const char *tag) {
    struct fake_snap *s = fake_snap_new(host, tag);
    MsgLock(msg);
    MsgReleaseTurboResult(msg);
    msg->turbo_result = s;
    msg->turbo_result_free = fake_free;
    msg->turbo_result_to_json = fake_to_json;
    msg->turbo_result_get_str = fake_get_str;
    MsgUnlock(msg);
}

static smsg_t *new_msg(void) {
    smsg_t *msg = NULL;
    CHECK(msgConstruct(&msg) == RS_RET_OK);
    return msg;
}

/* Always returns a heap string the caller frees; "" when absent. */
static char *get_str(smsg_t *msg, msgPropDescr_t *prop) {
    uchar *res = NULL;
    rs_size_t buflen = 0;
    unsigned short must_free = 0;
    char *out;

    CHECK(getJSONPropVal(msg, prop, &res, &buflen, &must_free) == RS_RET_OK);
    CHECK(res != NULL);
    out = strdup((char *)res);
    CHECK(out != NULL);
    if (must_free) free(res);
    return out;
}

/* msgGetJSONPropJSONorString: string fields come back as pcstr. */
static char *get_str_or_json(smsg_t *msg, msgPropDescr_t *prop) {
    struct json_object *json = NULL;
    uchar *cstr = NULL;
    char *out;
    rsRetVal r;

    r = msgGetJSONPropJSONorString(msg, prop, &json, &cstr);
    if (r != RS_RET_OK) {
        CHECK(json == NULL && cstr == NULL);
        return strdup("");
    }
    if (cstr != NULL) {
        out = strdup((char *)cstr);
        free(cstr);
    } else if (json != NULL) {
        out = strdup(json_object_to_json_string(json));
        json_object_put(json);
    } else {
        out = strdup("");
    }
    CHECK(out != NULL);
    return out;
}

static void expect_str(smsg_t *msg, msgPropDescr_t *prop, const char *want) {
    char *got = get_str(msg, prop);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s:%d: property '%s': want '%s' got '%s'\n", __FILE__, __LINE__, prop->name, want, got);
        abort();
    }
    free(got);
}

static void expect_contains(smsg_t *msg, msgPropDescr_t *prop, const char *needle) {
    char *got = get_str(msg, prop);
    if (strstr(got, needle) == NULL) {
        fprintf(stderr, "%s:%d: property '%s': '%s' not in '%s'\n", __FILE__, __LINE__, prop->name, needle, got);
        abort();
    }
    free(got);
}

static void expect_missing(smsg_t *msg, msgPropDescr_t *prop, const char *needle) {
    char *got = get_str(msg, prop);
    if (strstr(got, needle) != NULL) {
        fprintf(stderr, "%s:%d: property '%s': '%s' unexpectedly in '%s'\n", __FILE__, __LINE__, prop->name, needle,
                got);
        abort();
    }
    free(got);
}

/* ---- single-threaded contract ------------------------------------------ */

static void test_turbo_only_field_getters(void) {
    smsg_t *msg = new_msg();
    char *s;

    attach_snap(msg, "ubuntu", "tag1");
    expect_str(msg, &prop_host, "ubuntu");
    expect_str(msg, &prop_tag, "tag1");
    s = get_str_or_json(msg, &prop_host);
    CHECK(strcmp(s, "ubuntu") == 0);
    free(s);
    /* full tree materializes from the snapshot */
    expect_contains(msg, &prop_root, "\"host\"");
    expect_contains(msg, &prop_root, "ubuntu");
    msgDestruct(&msg);
}

static void test_jsonfind_sees_turbo_only_tree(void) {
    smsg_t *msg = new_msg();
    struct json_object *found = NULL;
    struct json_object *field = NULL;

    attach_snap(msg, "ubuntu", "");
    CHECK(jsonFind(msg, &prop_root, &found) == RS_RET_OK);
    CHECK(found != NULL);
    CHECK(json_object_object_get_ex(found, "host", &field));
    CHECK(strcmp(json_object_get_string(field), "ubuntu") == 0);
    found = NULL;
    CHECK(jsonFind(msg, &prop_host, &found) == RS_RET_OK);
    CHECK(found != NULL);
    CHECK(strcmp(json_object_get_string(found), "ubuntu") == 0);
    msgDestruct(&msg);
}

static void test_exists_on_turbo_only(void) {
    smsg_t *msg = new_msg();

    attach_snap(msg, "ubuntu", "");
    CHECK(msgCheckVarExists(msg, &prop_host) == RS_RET_OK);
    CHECK(msgCheckVarExists(msg, &prop_tag) == RS_RET_NOT_FOUND);
    msgDestruct(&msg);
}

static void test_prop_json_roots(void) {
    smsg_t *msg = new_msg();
    struct json_object *json = NULL;
    struct json_object *field = NULL;

    attach_snap(msg, "ubuntu", "");
    CHECK(msgGetJSONPropJSON(msg, &prop_root, &json) == RS_RET_OK);
    CHECK(json != NULL);
    CHECK(json_object_object_get_ex(json, "host", &field));
    CHECK(strcmp(json_object_get_string(field), "ubuntu") == 0);
    json_object_put(json);
    /* the deep copy must not alias the message tree */
    json = NULL;
    CHECK(msgGetJSONPropJSON(msg, &prop_root, &json) == RS_RET_OK);
    CHECK(json != NULL);
    json_object_object_add(json, "host", json_object_new_string("mutated"));
    json_object_put(json);
    expect_str(msg, &prop_host, "ubuntu");
    /* an empty local root is a valid request, not an error */
    json = NULL;
    CHECK(msgGetJSONPropJSON(msg, &prop_local, &json) == RS_RET_OK);
    CHECK(json == NULL);
    msgDestruct(&msg);
}

static void test_jsonmesg_carries_turbo_fields(void) {
    smsg_t *msg = new_msg();
    const uchar *jm;
    struct json_object *parsed;
    struct json_object *cee = NULL;
    struct json_object *field = NULL;

    attach_snap(msg, "ubuntu", "");
    jm = msgGetJSONMESG(msg);
    CHECK(jm != NULL);
    parsed = json_tokener_parse((const char *)jm);
    CHECK(parsed != NULL);
    CHECK(json_object_object_get_ex(parsed, "$!", &cee));
    CHECK(cee != NULL);
    CHECK(json_object_get_type(cee) == json_type_object);
    CHECK(json_object_object_get_ex(cee, "host", &field));
    CHECK(strcmp(json_object_get_string(field), "ubuntu") == 0);
    json_object_put(parsed);
    free((void *)jm);
    msgDestruct(&msg);
}

static void test_unset_root_turbo_only(void) {
    smsg_t *msg = new_msg();

    attach_snap(msg, "ubuntu", "tag1");
    CHECK(msgDelJSON(msg, (uchar *)"!") == RS_RET_OK);
    expect_str(msg, &prop_host, "");
    expect_str(msg, &prop_tag, "");
    expect_missing(msg, &prop_root, "ubuntu");
    CHECK(msgCheckVarExists(msg, &prop_host) == RS_RET_NOT_FOUND);
    msgDestruct(&msg);
}

static void test_unset_root_after_materialize(void) {
    smsg_t *msg = new_msg();

    attach_snap(msg, "ubuntu", "tag1");
    expect_contains(msg, &prop_root, "ubuntu"); /* materialize */
    CHECK(msgDelJSON(msg, (uchar *)"!") == RS_RET_OK);
    expect_str(msg, &prop_host, "");
    expect_missing(msg, &prop_root, "ubuntu");
    msgDestruct(&msg);
}

static void test_unset_leaf_turbo_only(void) {
    smsg_t *msg = new_msg();

    attach_snap(msg, "ubuntu", "tag1");
    CHECK(msgDelJSON(msg, (uchar *)"!host") == RS_RET_OK);
    expect_str(msg, &prop_host, "");
    expect_str(msg, &prop_tag, "tag1");
    expect_missing(msg, &prop_root, "ubuntu");
    expect_contains(msg, &prop_root, "tag1");
    msgDestruct(&msg);
}

static void test_second_attach_merges_into_materialized_tree(void) {
    smsg_t *msg = new_msg();

    attach_snap(msg, "ubuntu", "tag1");
    expect_contains(msg, &prop_root, "ubuntu"); /* materialize A */
    attach_snap(msg, "debian", "");
    /* later normalization wins on colliding keys, A-only keys survive */
    expect_str(msg, &prop_host, "debian");
    expect_str(msg, &prop_tag, "tag1");
    expect_contains(msg, &prop_root, "debian");
    expect_missing(msg, &prop_root, "ubuntu");
    msgDestruct(&msg);
}

static void test_second_attach_on_turbo_only_replaces(void) {
    smsg_t *msg = new_msg();

    attach_snap(msg, "ubuntu", "tag1");
    attach_snap(msg, "debian", "");
    expect_str(msg, &prop_host, "debian");
    expect_str(msg, &prop_tag, "");
    expect_missing(msg, &prop_root, "tag1");
    msgDestruct(&msg);
}

static void test_dup_shares_snapshot(void) {
    smsg_t *src = new_msg();
    smsg_t *dup;

    attach_snap(src, "ubuntu", "");
    dup = MsgDup(src);
    CHECK(dup != NULL);
    expect_str(src, &prop_host, "ubuntu");
    expect_str(dup, &prop_host, "ubuntu");
    /* overwriting the source must not disturb the copy */
    attach_snap(src, "debian", "");
    expect_str(src, &prop_host, "debian");
    expect_str(dup, &prop_host, "ubuntu");
    expect_contains(dup, &prop_root, "ubuntu");
    msgDestruct(&src);
    expect_str(dup, &prop_host, "ubuntu");
    msgDestruct(&dup);
}

static void test_dup_after_materialize(void) {
    smsg_t *src = new_msg();
    smsg_t *dup;

    attach_snap(src, "ubuntu", "tag1");
    expect_contains(src, &prop_root, "ubuntu");
    dup = MsgDup(src);
    CHECK(dup != NULL);
    expect_str(dup, &prop_host, "ubuntu");
    expect_contains(dup, &prop_root, "tag1");
    CHECK(msgDelJSON(dup, (uchar *)"!host") == RS_RET_OK);
    expect_str(dup, &prop_host, "");
    expect_str(src, &prop_host, "ubuntu");
    msgDestruct(&dup);
    msgDestruct(&src);
}

/* tplToJSON is the JSON-passing output path (ommongodb, omjournal): subtree
 * and per-field templates must return owned deep copies carrying the turbo
 * fields, and must not retain extra references (LSan). */
static void test_tpltojson_subtree_and_field(void) {
    smsg_t *msg = new_msg();
    struct template tpl;
    struct templateEntry tpe;
    struct json_object *json = NULL;
    struct json_object *field = NULL;

    attach_snap(msg, "ubuntu", "");

    /* template(type="subtree" subtree="$!") */
    memset(&tpl, 0, sizeof(tpl));
    tpl.bHaveSubtree = 1;
    CHECK(msgPropDescrFill(&tpl.subtree, (uchar *)"$!", 2) == RS_RET_OK);
    CHECK(tplToJSON(&tpl, msg, &json, NULL) == RS_RET_OK);
    CHECK(json != NULL);
    CHECK(json_object_object_get_ex(json, "host", &field));
    CHECK(strcmp(json_object_get_string(field), "ubuntu") == 0);
    /* owned copy: mutating it must not touch the message */
    json_object_object_add(json, "host", json_object_new_string("mutated"));
    json_object_put(json);
    expect_str(msg, &prop_host, "ubuntu");
    msgPropDescrDestruct(&tpl.subtree);

    /* template(type="list") { property(outname="h" name="$!host") } */
    memset(&tpl, 0, sizeof(tpl));
    memset(&tpe, 0, sizeof(tpe));
    tpe.eEntryType = FIELD;
    tpe.fieldName = (uchar *)"h";
    CHECK(msgPropDescrFill(&tpe.data.field.msgProp, (uchar *)"$!host", 6) == RS_RET_OK);
    tpl.pEntryRoot = &tpe;
    tpl.pEntryLast = &tpe;
    tpl.tpenElements = 1;
    json = NULL;
    CHECK(tplToJSON(&tpl, msg, &json, NULL) == RS_RET_OK);
    CHECK(json != NULL);
    CHECK(json_object_object_get_ex(json, "h", &field));
    CHECK(strcmp(json_object_get_string(field), "ubuntu") == 0);
    json_object_put(json); /* releases the per-field copy with the object */
    msgPropDescrDestruct(&tpe.data.field.msgProp);
    msgDestruct(&msg);
}

/* Same two template shapes on a message whose $! is a plain JSON tree (no
 * snapshot), the path every non-turbo build takes. */
static void test_tpltojson_plain_json(void) {
    smsg_t *msg = new_msg();
    struct template tpl;
    struct templateEntry tpe;
    struct json_object *json = NULL;
    struct json_object *field = NULL;
    struct json_object *tree;

    tree = json_object_new_object();
    CHECK(tree != NULL);
    json_object_object_add(tree, "host", json_object_new_string("plain"));
    CHECK(msgAddJSON(msg, (uchar *)"!", tree, 0, 0) == RS_RET_OK);
    expect_str(msg, &prop_host, "plain");

    memset(&tpl, 0, sizeof(tpl));
    tpl.bHaveSubtree = 1;
    CHECK(msgPropDescrFill(&tpl.subtree, (uchar *)"$!", 2) == RS_RET_OK);
    CHECK(tplToJSON(&tpl, msg, &json, NULL) == RS_RET_OK);
    CHECK(json != NULL);
    CHECK(json_object_object_get_ex(json, "host", &field));
    CHECK(strcmp(json_object_get_string(field), "plain") == 0);
    json_object_object_add(json, "host", json_object_new_string("mutated"));
    json_object_put(json);
    expect_str(msg, &prop_host, "plain");
    msgPropDescrDestruct(&tpl.subtree);

    memset(&tpl, 0, sizeof(tpl));
    memset(&tpe, 0, sizeof(tpe));
    tpe.eEntryType = FIELD;
    tpe.fieldName = (uchar *)"h";
    CHECK(msgPropDescrFill(&tpe.data.field.msgProp, (uchar *)"$!host", 6) == RS_RET_OK);
    tpl.pEntryRoot = &tpe;
    tpl.pEntryLast = &tpe;
    tpl.tpenElements = 1;
    json = NULL;
    CHECK(tplToJSON(&tpl, msg, &json, NULL) == RS_RET_OK);
    CHECK(json != NULL);
    CHECK(json_object_object_get_ex(json, "h", &field));
    CHECK(strcmp(json_object_get_string(field), "plain") == 0);
    json_object_put(json);
    /* a missing field on a non-empty tree is reported, not invented */
    msgPropDescrDestruct(&tpe.data.field.msgProp);
    CHECK(msgPropDescrFill(&tpe.data.field.msgProp, (uchar *)"$!absent", 8) == RS_RET_OK);
    json = NULL;
    CHECK(tplToJSON(&tpl, msg, &json, NULL) == RS_RET_OK);
    CHECK(json != NULL);
    CHECK(!json_object_object_get_ex(json, "h", &field));
    json_object_put(json);
    msgPropDescrDestruct(&tpe.data.field.msgProp);
    msgDestruct(&msg);
}

/* ---- threaded sharing ---------------------------------------------------- */

struct shared_arg {
    smsg_t *msg; /* each reader holds its own MsgAddRef reference */
    int iters;
    int kind;
};

static void check_host_value(const char *got) {
    if (strcmp(got, "ubuntu") != 0 && strcmp(got, "debian") != 0) {
        fprintf(stderr, "reader observed torn or empty host value '%s'\n", got);
        abort();
    }
}

/* Action-worker style reader: holds a reference to the same smsg_t and reads
 * through every $! path while the owner thread keeps replacing the snapshot.
 * kind selects the access path so every getter is exercised concurrently. */
static void *shared_reader(void *argp) {
    struct shared_arg *arg = argp;
    smsg_t *msg = arg->msg;
    int i;

    for (i = 0; i < arg->iters; ++i) {
        switch ((arg->kind + i) % 5) {
            case 0: {
                char *s = get_str(msg, &prop_host);
                check_host_value(s);
                free(s);
                break;
            }
            case 1: {
                char *s = get_str_or_json(msg, &prop_host);
                check_host_value(s);
                free(s);
                break;
            }
            case 2: {
                struct json_object *json = NULL;
                CHECK(msgGetJSONPropJSON(msg, &prop_root, &json) == RS_RET_OK);
                CHECK(json != NULL);
                json_object_put(json);
                break;
            }
            case 3: {
                const uchar *jm = msgGetJSONMESG(msg);
                CHECK(jm != NULL);
                CHECK(strstr((const char *)jm, "\"host\"") != NULL);
                free((void *)jm);
                break;
            }
            default:
                CHECK(msgCheckVarExists(msg, &prop_host) == RS_RET_OK);
                break;
        }
    }
    msgDestruct(&msg); /* drop this reader's reference */
    return NULL;
}

/* Queued-ruleset style worker: duplicates the shared message and reads the
 * copy, exercising first-counter publication and shared snapshot release. */
static void *dup_worker(void *argp) {
    struct shared_arg *arg = argp;
    int i;

    for (i = 0; i < arg->iters; ++i) {
        smsg_t *dup = MsgDup(arg->msg);
        CHECK(dup != NULL);
        if (i & 1) {
            char *s = get_str(dup, &prop_host);
            check_host_value(s);
            free(s);
        } else {
            expect_contains(dup, &prop_root, "\"host\"");
        }
        msgDestruct(&dup);
    }
    msgDestruct(&arg->msg);
    return NULL;
}

static int g_iters = DEFAULT_ITERS;

static void test_shared_readers_vs_overwrite(void) {
    const int iters = g_iters;
    smsg_t *src = new_msg();
    pthread_t readers[NREADERS];
    pthread_t duppers[NDUPPERS];
    struct shared_arg rarg[NREADERS];
    struct shared_arg darg[NDUPPERS];
    int overwrites = iters / 20;
    int i;

    if (overwrites < 200) overwrites = 200;
    attach_snap(src, "ubuntu", "");

    for (i = 0; i < NREADERS; ++i) {
        rarg[i].msg = MsgAddRef(src);
        rarg[i].iters = iters;
        rarg[i].kind = i;
        CHECK(pthread_create(&readers[i], NULL, shared_reader, &rarg[i]) == 0);
    }
    for (i = 0; i < NDUPPERS; ++i) {
        darg[i].msg = MsgAddRef(src);
        darg[i].iters = iters / 4;
        darg[i].kind = i;
        CHECK(pthread_create(&duppers[i], NULL, dup_worker, &darg[i]) == 0);
    }

    /* owner thread: second mmnormalize action keeps replacing the snapshot */
    for (i = 0; i < overwrites; ++i) {
        attach_snap(src, (i & 1) ? "debian" : "ubuntu", "");
    }

    for (i = 0; i < NREADERS; ++i) CHECK(pthread_join(readers[i], NULL) == 0);
    for (i = 0; i < NDUPPERS; ++i) CHECK(pthread_join(duppers[i], NULL) == 0);

    {
        char *s = get_str(src, &prop_host);
        check_host_value(s);
        free(s);
    }
    msgDestruct(&src);
}

/* Put the message back into the turbo-only state (no json tree, fresh
 * snapshot) in one locked step, so readers never observe a window without
 * any $! data. Lets the NULL -> tree transition recur under load. */
static void reset_to_turbo_only(smsg_t *msg, const char *host) {
    struct fake_snap *s = fake_snap_new(host, "");
    MsgLock(msg);
    if (msg->json != NULL) {
        json_object_put(msg->json);
        msg->json = NULL;
    }
    MsgReleaseTurboResult(msg);
    msg->turbo_result = s;
    msg->turbo_result_free = fake_free;
    msg->turbo_result_to_json = fake_to_json;
    msg->turbo_result_get_str = fake_get_str;
    MsgUnlock(msg);
}

/* Readers on the field path while another holder materializes the tree
 * (pMsg->json goes from NULL to a tree under the mutex). */
static void *materializer(void *argp) {
    struct shared_arg *arg = argp;
    int i;

    for (i = 0; i < arg->iters; ++i) {
        struct json_object *json = NULL;
        CHECK(msgGetJSONPropJSON(arg->msg, &prop_root, &json) == RS_RET_OK);
        CHECK(json != NULL);
        json_object_put(json);
        if ((i % 64) == 63) reset_to_turbo_only(arg->msg, (i & 64) ? "debian" : "ubuntu");
    }
    msgDestruct(&arg->msg);
    return NULL;
}

static void test_shared_readers_vs_materialize(void) {
    const int iters = g_iters;
    smsg_t *src = new_msg();
    pthread_t readers[NREADERS];
    pthread_t mat;
    struct shared_arg rarg[NREADERS];
    struct shared_arg marg;
    int i;

    attach_snap(src, "ubuntu", "");
    for (i = 0; i < NREADERS; ++i) {
        rarg[i].msg = MsgAddRef(src);
        rarg[i].iters = iters;
        rarg[i].kind = i;
        CHECK(pthread_create(&readers[i], NULL, shared_reader, &rarg[i]) == 0);
    }
    marg.msg = MsgAddRef(src);
    marg.iters = iters / 4;
    marg.kind = 0;
    CHECK(pthread_create(&mat, NULL, materializer, &marg) == 0);

    for (i = 0; i < NREADERS; ++i) CHECK(pthread_join(readers[i], NULL) == 0);
    CHECK(pthread_join(mat, NULL) == 0);
    msgDestruct(&src);
}

struct unit_test {
    const char *name;
    void (*fn)(void);
};

static const struct unit_test unit_tests[] = {
    {"turbo_only_field_getters", test_turbo_only_field_getters},
    {"jsonfind_sees_turbo_only_tree", test_jsonfind_sees_turbo_only_tree},
    {"exists_on_turbo_only", test_exists_on_turbo_only},
    {"prop_json_roots", test_prop_json_roots},
    {"jsonmesg_carries_turbo_fields", test_jsonmesg_carries_turbo_fields},
    {"unset_root_turbo_only", test_unset_root_turbo_only},
    {"unset_root_after_materialize", test_unset_root_after_materialize},
    {"unset_leaf_turbo_only", test_unset_leaf_turbo_only},
    {"second_attach_merges_into_materialized_tree", test_second_attach_merges_into_materialized_tree},
    {"second_attach_on_turbo_only_replaces", test_second_attach_on_turbo_only_replaces},
    {"dup_shares_snapshot", test_dup_shares_snapshot},
    {"dup_after_materialize", test_dup_after_materialize},
    {"tpltojson_subtree_and_field", test_tpltojson_subtree_and_field},
    {"tpltojson_plain_json", test_tpltojson_plain_json},
    {"shared_readers_vs_overwrite", test_shared_readers_vs_overwrite},
    {"shared_readers_vs_materialize", test_shared_readers_vs_materialize},
};

/* TURBO_MSGDUP_TEST=<name> runs a single check, TURBO_MSGDUP_ITERS=<n>
 * scales the threaded ones. */
int main(void) {
    const char *errObj = NULL;
    obj_if_t objIF;
    const char *iters_env;
    const char *only;
    size_t i;
    int ran = 0;

    /* runtime helper modules come from the build tree, as in testbench.h */
    setenv("RSYSLOG_MODDIR", "../runtime/.libs/", 0);
    memset(&objIF, 0, sizeof(objIF));
    CHECK(rsrtInit(&errObj, &objIF) == RS_RET_OK);

    memset(&prop_root, 0, sizeof(prop_root));
    memset(&prop_host, 0, sizeof(prop_host));
    memset(&prop_tag, 0, sizeof(prop_tag));
    memset(&prop_local, 0, sizeof(prop_local));
    CHECK(msgPropDescrFill(&prop_root, (uchar *)"$!", 2) == RS_RET_OK);
    CHECK(msgPropDescrFill(&prop_host, (uchar *)"$!host", 6) == RS_RET_OK);
    CHECK(msgPropDescrFill(&prop_tag, (uchar *)"$!tag", 5) == RS_RET_OK);
    CHECK(msgPropDescrFill(&prop_local, (uchar *)"$.", 2) == RS_RET_OK);

    iters_env = getenv("TURBO_MSGDUP_ITERS");
    if (iters_env != NULL && iters_env[0] != '\0') {
        char *end = NULL;
        long v = strtol(iters_env, &end, 10);
        CHECK(end != iters_env && v > 0 && v < 10000000);
        g_iters = (int)v;
    }

    only = getenv("TURBO_MSGDUP_TEST");
    for (i = 0; i < sizeof(unit_tests) / sizeof(unit_tests[0]); ++i) {
        if (only != NULL && only[0] != '\0' && strcmp(only, unit_tests[i].name) != 0) continue;
        printf("%s\n", unit_tests[i].name);
        fflush(stdout);
        unit_tests[i].fn();
        ++ran;
    }
    CHECK(ran > 0);

    msgPropDescrDestruct(&prop_root);
    msgPropDescrDestruct(&prop_host);
    msgPropDescrDestruct(&prop_tag);
    msgPropDescrDestruct(&prop_local);
    CHECK(rsrtExit() == RS_RET_OK);
    return 0;
}
#endif
