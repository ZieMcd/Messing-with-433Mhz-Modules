#include <lgpio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO 27

typedef struct {
  int level;
  uint64_t timestamp;
} evt_t;

typedef struct {
  double duration;
  int count;
} pulse_t;

typedef struct {
  int cap;
  int len;
  evt_t *evt_p;
  pthread_mutex_t lock;
} evt_list_t;

void addEvt(evt_list_t *evt_list, evt_t event) {
  pthread_mutex_lock(&evt_list->lock);
  if (evt_list->len == evt_list->cap) {
    evt_list->cap *= 2;
    evt_list->evt_p = realloc(evt_list->evt_p, evt_list->cap * sizeof(evt_t));
    if (evt_list->evt_p == NULL) {
      printf("Error when trying to expand list \n");
      exit(EXIT_FAILURE);
    }
  }
  evt_list->evt_p[evt_list->len] = event;
  evt_list->len += 1;
  pthread_mutex_unlock(&evt_list->lock);
}

int getGpioChipHandle() {
  lgChipInfo_t cInfo;
  int gpio_num = 0;
  int h;

open_chip:
  h = lgGpiochipOpen(gpio_num);
  if (h >= 0) {
    int status = lgGpioGetChipInfo(h, &cInfo);
    if (status == LG_OKAY && strcmp(cInfo.label, "pinctrl-rp1") == 0)
      return h;
    lgGpiochipClose(h);
  }
  if (gpio_num == 0) {
    gpio_num = 4;
    goto open_chip;
  }
  fprintf(stderr, "RP1 GPIO chip not found\n");
  exit(EXIT_FAILURE);
}

void onAlertFunc(int e, lgGpioAlert_p evt_p, void *data) {

  for (int i = 0; i < e; i++) {
    lgGpioAlert_t evn_t = evt_p[i];
    lgGpioReport_t report = evn_t.report;
    evt_t evt = {report.level, report.timestamp};
    addEvt(data, evt);
  }
}

int main(int argc, char *argv[]) {
  int h = getGpioChipHandle();

  evt_list_t evt_list = {10, 0, malloc(10 * sizeof(evt_t))};
  pthread_mutex_init(&evt_list.lock, NULL);

  if (lgGpioSetAlertsFunc(h, GPIO, onAlertFunc, &evt_list) < 0) {
    printf("failed to set alert \n");
    exit(EXIT_FAILURE);
  }

  if (lgGpioClaimAlert(h, 0, LG_BOTH_EDGES, GPIO, -1) < 0) {
    printf("failed to cliam alert\n");
    exit(EXIT_FAILURE);
  }
  lguSleep(5);
  lgGpiochipClose(h);

  pthread_mutex_lock(&evt_list.lock);

  FILE *f = fopen("pulses.txt", "w");
  if (f == NULL) {
    printf("failed to open file");
    exit(EXIT_FAILURE);
  }

  for (int i = 1; i < evt_list.len; i++) {
    evt_t cur_evt = evt_list.evt_p[i];
    evt_t prev_evt = evt_list.evt_p[i - 1];
    char level;

    if (prev_evt.level == 1)
      level = 'H';
    else
      level = 'L';

    uint64_t timebetween_miliseconds =
        (cur_evt.timestamp - prev_evt.timestamp) / 1000;

    fprintf(f, "%c : %" PRIu64 "μs\n", level, timebetween_miliseconds);
  }

  pthread_mutex_unlock(&evt_list.lock);
  fclose(f);

  printf("%d", evt_list.len);
}
