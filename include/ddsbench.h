
#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "c" {
#endif

typedef struct ddsbench_context {
    char *qos;
    char *filter;
    unsigned int payload;
    unsigned int burstsize;
    unsigned int burstinterval;
    unsigned int pollingdelay;
    unsigned int subid;
    unsigned int pubid;
    unsigned int topicid;
    char topicname[256];
    char filtername[256];
} ddsbench_context;

typedef struct ddsbench_threadArg {
    int id;
    char topicName[256];
    ddsbench_context *ctx;
} ddsbench_threadArg;

typedef struct ddsbench_libraryInterface {
    /* Product initialization */
    int (*init)(ddsbench_context *ctx);
    void (*fini)(void);

    /* Thread callbacks */
    void* (*lpub)(void *ctx);
    void* (*lsub)(void *ctx);
    void* (*tpub)(void *ctx);
    void* (*tsub)(void *ctx);

    /* Pointer to library */
    void *lib;
} ddsbench_libraryInterface;

#ifdef __cplusplus
}
#endif

#endif
