#include "client.h"
#include "experiment_harness.h"

int main(void) {
  CLIENT_init();
  perform_experiments();

  return 0;
}
