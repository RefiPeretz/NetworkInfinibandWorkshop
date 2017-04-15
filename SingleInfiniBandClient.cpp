//
// Created by fimka on 15/04/17.
//

#include <infiniband/verbs.h>

class SingleQPStremCliient
{
  int                      ib_port = 1;
  int                      size = 4096;
  int                      rx_depth = 500;
  int                      use_event = 0;
  char                    *servername = NULL;


  int main(int argc, char *argv[])
  {

	//get the device list on the clinet
	ibv_device ** dev_list = ibv_get_device_list(NULL);

	//Get device from list.
	ib_dev = *dev_list;
	if (!ib_dev) {
	  fprintf(stderr, "No IB devices found\n");
	  return 1;
	}
	/*
	 * creates connection
	 */
	//"ctx" Holds the whole connection data
	struct pingpong_context *ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event, !servername);
	if (!ctx)
	{
	  return 1;
	}



	ibv_free_device_list(dev_list); //Only after we've opened a device



	return 0;
  }



};

