#ifndef __KU_C__
#define __KU_C__

#define KU_CREAT			0x010
#define KU_EXCL				0x020
#define KU_NOWAIT			0x040
#define KU_MSG_NOERROR			0x100

#define KU_MSGMAX			128

#define KU_DEV_NAME			"ku_sa"
#define KU_DEV_PATH 			"/dev/"KU_DEV_NAME

#define KU_MINOR			0xC1
#define KU_OP				0x80

struct msg_buf {
	unsigned int distance; // cm
	char timestamp[KU_MSGMAX];
};

typedef struct _KU_READWRITE {
	struct msg_buf *msgp;
	int msgsz;
	int msgflg;
} KU_READWRITE;

#define KU_OPEN				_IOWR(KU_MINOR,KU_OP+1,KU_READWRITE)
#define KU_CLOSE			_IOWR(KU_MINOR,KU_OP+2,KU_READWRITE)
#define KU_READ				_IOWR(KU_MINOR,KU_OP+3,KU_READWRITE)
#define KU_WRITE			_IOWR(KU_MINOR,KU_OP+4,KU_READWRITE)

#define forn(i, n)			for (i = 0; i < (int) (n); i++)

#endif // __KU_C__
