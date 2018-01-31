namespace usbd {

	const int DEVICE_DESCRIPTOR = 1;
	const int DEVICE_DESCRIPTOR_SIZE = 18;

	const int CONFIGURATION_DESCRIPTOR = 2;
	const int CONFIGURATION_DESCRIPTOR_SIZE = 9;

	const int STRING_DESCRIPTOR = 3;

	const int INTERFACE_DESCRIPTOR = 4;
	const int INTERFACE_DESCRIPTOR_SIZE = 9;

	const int ENDPOINT_DESCRIPTOR = 5;
	const int ENDPOINT_DESCRIPTOR_SIZE = 7;

	const int GET_STATUS = 0x00;
	const int GET_DESCRIPTOR = 0x06;
	const int SET_ADDRESS = 0x05;
	const int GET_CONFIGURATION = 0x08;
	const int SET_CONFIGURATION = 0x09;

	class DefaultEndpointCallback {
	public:
		virtual void sendDeviceDescriptor() = 0;
		virtual void sendConfigurationDescriptor(int index, int len) = 0;
		virtual void sendStringDescriptor(int index) = 0;
		virtual int getConfiguration() = 0;
		virtual void setConfiguration(int configIndex) = 0;
		virtual void handleNonStandardRequest(SetupPacket* packet, int counter, unsigned char* data, int len) = 0;
	};

	class DefaultEndpoint : public AbstractControlEndpoint {
	private:
		SetupPacket lastSetupPacket;
		int transferInCounter;
		int transferOutCounter;
		int sendMore;

	public:
		DefaultEndpointCallback* callback;
		int addressToBeSet = 0;

		virtual void init() {
			AbstractControlEndpoint::init();
		}

		virtual void correctTransferSetup(unsigned char* data, int len) {

			lastSetupPacket = *(SetupPacket*) data;
			transferInCounter = 0;
			transferOutCounter = 0;

			handleSetupPacket(&lastSetupPacket, transferOutCounter, 0, 0);
			statRx(EP_VALID);
		}

		virtual void correctTransferOut(unsigned char* data, int len) {
			handleSetupPacket(&lastSetupPacket, ++transferOutCounter, data, len);
			statRx(EP_VALID);
		}

		void handleSetupPacket(SetupPacket* packet, int counter, unsigned char* data, int len) {
			int request = packet->bRequest;

			if (request == GET_STATUS) {
				unsigned char data[] = {0, 0};
				send(data, sizeof (data));

			} else if (request == GET_DESCRIPTOR) {
				int type = packet->wValue >> 8;
				int index = packet->wValue & 0xFF;

				if (type == DEVICE_DESCRIPTOR) {
					callback->sendDeviceDescriptor();
				} else if (type == CONFIGURATION_DESCRIPTOR) {
					callback->sendConfigurationDescriptor(index, packet->wLength);
				} else if (type == STRING_DESCRIPTOR) {
					callback->sendStringDescriptor(index);
				} else {
					sendZLP();
				}

			} else if (request == SET_ADDRESS) {
				addressToBeSet = packet->wValue;
				sendZLP();

			} else if (request == GET_CONFIGURATION) {
				unsigned char data = callback->getConfiguration();
				send(&data, sizeof (data));
			} else if (request == SET_CONFIGURATION) {
				callback->setConfiguration(packet->wValue);
				sendZLP();
			} else {
				callback->handleNonStandardRequest(packet, counter, data, len);
			}

		}

		void sendLong(unsigned char* data, int len) {
			int offset = txBufferSize * transferInCounter;
			len -= offset;
			if (len > txBufferSize) {
				len = txBufferSize;
			}
			sendMore = len == txBufferSize;
			send(&data[offset], len);
		}

		virtual void correctTransferIn() {

			if (addressToBeSet != 0) {
				target::USB.DADDR.setADD(addressToBeSet);
				addressToBeSet = 0;
			}

			if (sendMore) {
				handleSetupPacket(&lastSetupPacket, ++transferInCounter, 0, 0);
			}
		}
	};

}