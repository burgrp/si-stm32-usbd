namespace usbd {

#if INTERFACE

#define CDC_CHAR_FORMAT_STOP_BIT_0_0 0 
#define CDC_CHAR_FORMAT_STOP_BIT_1_5 1 
#define CDC_CHAR_FORMAT_STOP_BIT_2_0 2 

#define CDC_PARITY_NONE 0
#define CDC_PARITY_ODD 1
#define CDC_PARITY_EVEN 2
#define CDC_PARITY_MARK 3
#define CDC_PARITY_SPACE 4 

#define CDC_DATA_OUT_BUFFER_LEN 64
#define CDC_DATA_IN_BUFFER_LEN 64

	class ComBridge {
	public:
		virtual void sendByteIn(unsigned char b) = 0;
		virtual void dataSentOut() = 0;
	};

	class ComPort {
	public:
		ComBridge* bridge;

		virtual void setLineCoding(int bitRate, int charFormat, int parity, int dataBits) = 0;
		virtual void getLineCoding(int* bitRate, int* charFormat, int* parity, int* dataBits) = 0;
		virtual void setControlLineState(int dtr, int rts) = 0;
		virtual void sendDataOut(unsigned char* data, int len) = 0;

		virtual void setBridge(ComBridge* bridge) {
			this->bridge = bridge;
		};
	};

	class UartBridgeNotificationEndpoint : public AbstractInterruptEndpoint {
	public:

		virtual void init() {
			AbstractInterruptEndpoint::init();
			rxBufferSize = 0;
			txBufferSize = 16;
		}
	};

	class UartBridgeDataInEndopoint : public AbstractBulkEndpoint {
		unsigned char txL2Buffer[CDC_DATA_IN_BUFFER_LEN];
		int txL2BufferLen;

	public:

		virtual void init() {
			rxBufferSize = 0;
			txBufferSize = CDC_DATA_IN_BUFFER_LEN;
			AbstractBulkEndpoint::init();
		}

		virtual void correctTransferIn();

	};

	class UartBridgeDataOutEndopoint : public AbstractBulkEndpoint {
	public:

		UartBridgeDataInterface* interface;

		virtual void init() {
			rxBufferSize = CDC_DATA_OUT_BUFFER_LEN;
			txBufferSize = 0;
			AbstractBulkEndpoint::init();
		}

		virtual void correctTransferOut(unsigned char* data, int len);
	};

	class UartBridgeCommInterface : public UsbInterface, public ComBridge {
	public:
		int interfaceNumber;
		UartBridgeDataInterface* dataInterface;

		UartBridgeNotificationEndpoint notificationEndpoint;

		ComPort* port;

		virtual void init() {
			endpoints[0] = &notificationEndpoint;

			interfaceClass = 0x02;
			interfaceSubClass = 0x02;
			interfaceProtocol = 0x00;

			UsbInterface::init();

		}

		virtual int getExtraDescriptorSize();
		virtual void writeExtraDescriptor(unsigned char** descriptor);
		virtual void sendByteIn(unsigned char b);
		virtual void dataSentOut();
	};

	class UartBridgeDataInterface : public UsbInterface {
	public:
		int interfaceNumber;
		UartBridgeCommInterface* commInterface;

		UartBridgeDataInEndopoint dataInEndpoint;
		UartBridgeDataOutEndopoint dataOutEndpoint;

		virtual void init() {
			endpoints[0] = &dataInEndpoint;
			endpoints[1] = &dataOutEndpoint;

			dataOutEndpoint.interface = this;

			interfaceClass = 0x0A;
			interfaceSubClass = 0x00;
			interfaceProtocol = 0x00;

			UsbInterface::init();
		}

	};

#endif

#define CS_UNDEFINED                    0x20
#define CS_DEVICE                       0x21
#define CS_CONFIGURATION                0x22
#define CS_STRING                       0x23
#define CS_INTERFACE                    0x24
#define CS_ENDPOINT                     0x25

	int UartBridgeCommInterface::getExtraDescriptorSize() {
		return 14;
	}

	void UartBridgeCommInterface::writeExtraDescriptor(unsigned char** descriptor) {
		*(*descriptor)++ = 0x05; //0 bFunctionLength 1 05h Size of this functional descriptor, in bytes.
		*(*descriptor)++ = 0x24; //1 bDescriptorType 1 24h CS_INTERFACE
		*(*descriptor)++ = 0x00; //2 bDescriptorSubtype 1 00h Header. This is defined in [USBCDC1.2], which defines this as a header.
		*(*descriptor)++ = 0x10;
		*(*descriptor)++ = 01; //3 bcdCDC 2 0110h USB Class Definitions for Communications Devices Specification release number in binary-coded decimal.

		*(*descriptor)++ = 0x04; //5 bFunctionLength 1 04h Size of this functional descriptor, in bytes. 
		*(*descriptor)++ = 0x24; //6 bDescriptorType 1 24h CS_INTERFACE
		*(*descriptor)++ = 0x02; //7 bDescriptorSubtype 1 02h Abstract Control Management functional descriptor subtype as defined in [USBCDC1.2].
		*(*descriptor)++ = 0x02; //8 bmCapabilities 1 

		*(*descriptor)++ = 0x05; //9 bFunctionLength 1 05h Size of this functional descriptor, in bytes
		*(*descriptor)++ = 0x24; //10 bDescriptorType 1 24h CS_INTERFACE
		*(*descriptor)++ = 0x06; //11 bDescriptorSubtype 1 06h Union Descriptor Functional Descriptor subtype as defined in [USBCDC1.2].
		*(*descriptor)++ = interfaceNumber; //12 bControlInterface 1 00h Interface number of the control (Communications Class) interface
		*(*descriptor)++ = dataInterface->interfaceNumber; //13 bSubordinateInterface0 1 01h Interface number of the subordinate (Data Class) interface
	}

#define SET_LINE_CODING 0x20
#define GET_LINE_CODING 0x21
#define SET_CONTROL_LINE_STATE 0x22

	void UartBridgeDataOutEndopoint::correctTransferOut(unsigned char* data, int len) {
		statRx(EP_NAK);
		interface->commInterface->port->sendDataOut(data, len);
	}

	PUBLIC void UartBridgeCommInterface::handleControlRequest(SetupPacket* packet, int counter, unsigned char* data, int len, DefaultEndpoint* defaultEndpoint) {

		if (packet->bmRequestType == 0b00100001 && packet->bRequest == SET_LINE_CODING && counter == 1) {

			int bitRate = *data++;
			bitRate |= *data++ << 8;
			bitRate |= *data++ << 16;
			bitRate |= *data++ << 24;

			int charFormat = *data++;
			int parity = *data++;
			int dataBits = *data++;

			port->setLineCoding(bitRate, charFormat, parity, dataBits);

			defaultEndpoint->sendZLP();

		} else if (packet->bmRequestType == 0b10100001 && packet->bRequest == GET_LINE_CODING) {

			int bitRate = 0;
			int charFormat = 0;
			int parity = 0;
			int dataBits = 0;

			port->getLineCoding(&bitRate, &charFormat, &parity, &dataBits);

			unsigned char data[7];
			data[0] = bitRate & 0xFF;
			data[1] = (bitRate >> 8) & 0xFF;
			data[2] = (bitRate >> 16) & 0xFF;
			data[3] = (bitRate >> 24) & 0xFF;
			data[4] = charFormat;
			data[5] = parity;
			data[6] = dataBits;
			defaultEndpoint->send(data, sizeof (data));

		} else if (packet->bmRequestType == 0b00100001 && packet->bRequest == SET_CONTROL_LINE_STATE) {

			int dtr = packet->wValue & 1;
			int rts = (packet->wValue > 1) & 1;

			port->setControlLineState(dtr, rts);

			defaultEndpoint->sendZLP();

		} else {
			defaultEndpoint->sendZLP();
		}

	}

	PUBLIC void UartBridgeCommInterface::setPort(ComPort* port) {
		this->port = port;
		port->setBridge(this);
	}

	void UartBridgeCommInterface::sendByteIn(unsigned char b) {
		dataInterface->dataInEndpoint.sendByteIn(b);
	}

	PUBLIC void UartBridgeDataInEndopoint::sendByteIn(unsigned char b) {
		if (txL2BufferLen < sizeof (txL2Buffer)) {
			txL2Buffer[txL2BufferLen++] = b;
		}

		if (epr->STAT_TX() == EP_NAK) {
			correctTransferIn();
		}
	}

	void UartBridgeDataInEndopoint::correctTransferIn() {
		if (txL2BufferLen) {
			int l = txL2BufferLen;
			txL2BufferLen = 0;
			send(txL2Buffer, l);
		}
	}

	void UartBridgeCommInterface::dataSentOut() {
		dataInterface->dataOutEndpoint.statRx(EP_VALID);
	}

}