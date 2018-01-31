namespace usbd {

	const int EP_DIS = 0x000; /* EndPoint DISabled */
	const int EP_STALL = 0x001; /* EndPoint STALLed */
	const int EP_NAK = 0x002; /* EndPoint NAKed */
	const int EP_VALID = 0x003; /* EndPoint VALID */

	const int EP_TYPE_CONTROL = 0;
	const int EP_TYPE_ISOCHRONOUS = 1;
	const int EP_TYPE_BULK = 2;
	const int EP_TYPE_INTERRUPT = 3;

	const int PACKET_MEMORY_BASE = 0x40006000;

	typedef struct UsbBufferDescriptor {
		short ADDR_TX, COUNT_TX, ADDR_RX, COUNT_RX;
	} UsbBufferDescriptor;

	typedef struct SetupPacket {
		unsigned char bmRequestType;
		unsigned char bRequest;
		unsigned short wValue;
		unsigned short wIndex;
		unsigned short wLength;
	} SetupPacket;

	class UsbEndpoint {
	public:
		int rxBufferSize = 64;
		int txBufferSize = 64;
		int epType;
		target::usb::reg::EP0R defaultEprVal;

		UsbBufferDescriptor* bufferDescriptor;
		volatile target::usb::reg::EP0R* epr;

		virtual void init() {
		};

		void correctTransfer() {

			if (epr->getCTR_RX()) {
				int len = bufferDescriptor->COUNT_RX & 0x3FF;


				unsigned short* srcShorts = (unsigned short*) (PACKET_MEMORY_BASE + bufferDescriptor->ADDR_RX);
				int shorts = (len >> 1) + (len & 1);
				unsigned short dstShorts[shorts];
				for (int c = 0; c < shorts; c++) {
					dstShorts[c] = srcShorts[c];
				}

				unsigned char* data = (unsigned char*) dstShorts;

				if (epr->getSETUP()) {
					correctTransferSetup(data, len);
				} else {
					correctTransferOut(data, len);
				}

			}

			if (epr->getCTR_TX()) {
				statTx(EP_NAK);
				correctTransferIn();
			}

		}

		void statRx(int stat) {
			target::usb::reg::EP0R e;
			e = defaultEprVal;
			e.setSTAT_RX(epr->getSTAT_RX() ^ stat);
			*epr = e;
		}

		void statTx(int stat) {
			target::usb::reg::EP0R e;
			e = defaultEprVal;
			e.setSTAT_TX(epr->getSTAT_TX() ^ stat);
			*epr = e;
		}

		void reset(int address) {

			static const int USB_EP_TYPE_TO_STM[] = {
				1, 2, 0, 3
			};

			defaultEprVal = 0;
			defaultEprVal.setEP_TYPE(USB_EP_TYPE_TO_STM[epType]);
			defaultEprVal.setEA(address);

			statRx(EP_VALID);
			statTx(EP_NAK);
		}

		void send(unsigned char* data, int len) {

			unsigned short* dstShorts = (unsigned short*) (PACKET_MEMORY_BASE + bufferDescriptor->ADDR_TX);
			unsigned short* srcShorts = (unsigned short*) data;
			for (int c = 0; c < (len >> 1) + (len & 1); c++) {
				dstShorts[c] = srcShorts[c];
			}

			bufferDescriptor->COUNT_TX = len;

			statTx(EP_VALID);
		}
		
		void sendZLP() {
			send(NULL, 0);
		}

		virtual void correctTransferSetup(unsigned char* data, int len) {
			statRx(EP_VALID);
		};

		virtual void correctTransferOut(unsigned char* data, int len) {
			statRx(EP_VALID);
		};

		virtual void correctTransferIn() {
		};

	};

	class AbstractControlEndpoint : public UsbEndpoint {
	public:

		virtual void init() {
			UsbEndpoint::init();
			epType = EP_TYPE_CONTROL;
		}
	};

	class AbstractBulkEndpoint : public UsbEndpoint {
	public:

		virtual void init() {
			UsbEndpoint::init();
			epType = EP_TYPE_BULK;
		}
	};

	class AbstractIsochronousEndpoint : public UsbEndpoint {
	public:

		virtual void init() {
			UsbEndpoint::init();
			epType = EP_TYPE_ISOCHRONOUS;
		}
	};

	class AbstractInterruptEndpoint : public UsbEndpoint {
	public:

		virtual void init() {
			UsbEndpoint::init();
			epType = EP_TYPE_INTERRUPT;
		}
	};

}