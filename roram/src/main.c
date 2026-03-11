#include "server.h"
#include "client.h"
#include "experiment_harness.h"

int main(void) {
  SERVER_init();
  CLIENT_init();
  perform_experiments();

  return 0;
}
