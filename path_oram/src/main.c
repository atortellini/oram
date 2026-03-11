#include "server.h"
#include "client.h"
#include "crypto.h"
#include "experiment_harness.h"

int main(void) {

  CRYPTO_init();
  SERVER_init();
  CLIENT_setup();
  perform_experiments();

  return 0;
}
