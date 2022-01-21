#pragma once
#include "Client.h"
namespace wm {
	
	//subcribers subscrib to publisers to publish data to them
	//template on the socket type ??? 

	class Publisher : public Client
	{
	public:
		virtual ~Publisher() {}

	private:

	};
}
