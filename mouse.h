#ifndef _CIOLIB_MOUSE_H_
#define _CIOLIB_MOUSE_H_

struct mouse_event {
	int event;
	int bstate;
	int kbsm;
	int startx;
	int starty;
	int endx;
	int endy;
};

#define CIOLIB_BUTTON_1	1
#define CIOLIB_BUTTON_2	2
#define CIOLIB_BUTTON_3	4

#define CIOLIB_BUTTON(x)	(1<<(x-1))

enum {
	 CIOLIB_MOUSE_MOVE
	,CIOLIB_BUTTON_1_PRESS
	,CIOLIB_BUTTON_1_RELEASE
	,CIOLIB_BUTTON_1_CLICK
	,CIOLIB_BUTTON_1_DBL_CLICK
	,CIOLIB_BUTTON_1_TRPL_CLICK
	,CIOLIB_BUTTON_1_QUAD_CLICK
	,CIOLIB_BUTTON_1_DRAG_START
	,CIOLIB_BUTTON_1_DRAG_MOVE
	,CIOLIB_BUTTON_1_DRAG_END
	,CIOLIB_BUTTON_2_PRESS
	,CIOLIB_BUTTON_2_RELEASE
	,CIOLIB_BUTTON_2_CLICK
	,CIOLIB_BUTTON_2_DBL_CLICK
	,CIOLIB_BUTTON_2_TRPL_CLICK
	,CIOLIB_BUTTON_2_QUAD_CLICK
	,CIOLIB_BUTTON_2_DRAG_START
	,CIOLIB_BUTTON_2_DRAG_MOVE
	,CIOLIB_BUTTON_2_DRAG_END
	,CIOLIB_BUTTON_3_PRESS
	,CIOLIB_BUTTON_3_RELEASE
	,CIOLIB_BUTTON_3_CLICK
	,CIOLIB_BUTTON_3_DBL_CLICK
	,CIOLIB_BUTTON_3_TRPL_CLICK
	,CIOLIB_BUTTON_3_QUAD_CLICK
	,CIOLIB_BUTTON_3_DRAG_START
	,CIOLIB_BUTTON_3_DRAG_MOVE
	,CIOLIB_BUTTON_3_DRAG_END
};

#define CIOLIB_BUTTON_PRESS(x)		((x-1)*9+1)
#define CIOLIB_BUTTON_RELEASE(x)	((x-1)*9+2)
#define CIOLIB_BUTTON_CLICK(x)		((x-1)*9+3)
#define CIOLIB_BUTTON_DBL_CLICK(x)	((x-1)*9+4)
#define CIOLIB_BUTTON_TRPL_CLICK(x)	((x-1)*9+5)
#define CIOLIB_BUTTON_QUAD_CLICK(x)	((x-1)*9+6)
#define CIOLIB_BUTTON_DRAG_START(x)	((x-1)*9+7)
#define CIOLIB_BUTTON_DRAG_MOVE(x)	((x-1)*9+8)
#define CIOLIB_BUTTON_DRAG_END(x)	((x-1)*9+9)

#define CIOLIB_BUTTON_NUMBER(x)		((x+8)/9)

#define CIOLIB_BUTTON_BASE(x)		(x!=CIOLIB_MOUSE_MOVE?x-9*(CIOLIB_BUTTON_NUMBER(x)-1):CIOLIB_MOUSE_MOVE)

#ifdef __cplusplus
extern "C" {
#endif
void ciomouse_gotevent(int event, int x, int y);
int mouse_pending(void);
int ciolib_getmouse(struct mouse_event *mevent);
void ciolib_mouse_thread(void *data);
int ciomouse_setevents(int events);
int ciomouse_addevents(int events);
int ciomouse_delevents(int events);
#ifdef __cplusplus
}
#endif

#endif
