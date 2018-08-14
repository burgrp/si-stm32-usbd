namespace usbd {

	const int MAX_CONFIGURATIONS = 8;

	const int STRING_DESCRIPTOR_VENDOR = 1;
	const int STRING_DESCRIPTOR_PRODUCT = 2;
	const int STRING_DESCRIPTOR_SERIAL_NUMBER = 3;
	const int STRING_DESCRIPTOR_INTERFACE = 0x40;

	class UsbDevice;
	UsbDevice* usbDevice;

	class UsbDevice : public DefaultEndpointCallback {
	public:

		int idVendor = 0x0483;
		int idProduct = 0xFEE0;
		int devRelease = 0x0100;

		const char* strVendor = NULL;
		const char* strProduct = NULL;
		const char* strSerialNo = NULL;

		int configIndex;

		UsbEndpoint* endpoints[MAX_ENDPOINTS];
		int numEndpoints;

		UsbConfiguration* configurations[MAX_CONFIGURATIONS];
		int numConfigurations;

		DefaultEndpoint defaultEndpoint;

		virtual void init() {

			// reset USB and CRS peripherals
			target::RCC.APB1RSTR.setCRSRST(1);
			target::RCC.APB1RSTR.setCRSRST(0);
			target::RCC.APB1RSTR.setUSBRST(1);
			target::RCC.APB1RSTR.setUSBRST(0);

			// D+ pull-up off
			target::USB.BCDR.setDPPU(0);

			// enable clock domains
			target::RCC.APB1ENR.setUSBRST(1);
			target::RCC.APB1ENR.setCRSEN(1);

			// enable internal 48MHz oscillator
			target::RCC.CR2.setHSI48ON(1);
			while (!target::RCC.CR2.getHSI48RDY());

			// system clock to HSI48
			target::RCC.CFGR.setSW(3);
			while (target::RCC.CFGR.getSWS() != 3);

			// core, AHB, DMA clock to 24MHz
			target::RCC.CFGR.setHPRE(0);

			// APB clock to 12MHz
			target::RCC.CFGR.setPPRE(0);

			// no need for HSI anymore
			//RCC.CR.HSION(0);

			// HSI48 to USB
			target::RCC.CFGR3.setUSBSW(0);

			// configure the Clock Recovery System to USB SOF
			target::CRS.CFGR.setSYNCSRC(2);
			target::CRS.CR.setAUTOTRIMEN(1);
			target::CRS.CR.setCEN(1);

			// power up USB analog
			target::USB.CNTR.setPDWN(0);

			// initialize endpoints and buffers
			target::USB.BTABLE.setBTABLE(0);

			numConfigurations = 0;
			for (int i = 0; i < MAX_CONFIGURATIONS && configurations[i]; i++) {
				configurations[i]->init();
				numConfigurations++;
			}

			setConfiguration(0);

			// enable USB interrupts
			target::USB.CNTR.setPDWN(0);

			target::USB.CNTR.setRESETM(1);
			target::USB.CNTR.setCTRM(1);

			usbDevice = this;
			target::USB.CNTR.setFRES(0);
			target::NVIC.ISER.setSETENA(1 << target::interrupts::External::USB);

			// D+ pull-up on
			target::USB.BCDR.setDPPU(1);
		}

		virtual int getConfiguration() {
			return this->configIndex;
		}

		virtual void setConfiguration(int configIndex) {

			// configIndex is 1 based, 0 means unconfigured

			this->configIndex = configIndex;

			defaultEndpoint.callback = this;

			endpoints[0] = &defaultEndpoint;
			numEndpoints = 1;

			for (int i = numEndpoints; i < MAX_ENDPOINTS; i++) {
				endpoints[i] = NULL;
			}

			if (configIndex > 0 && configIndex <= numConfigurations) {
				UsbConfiguration* configuration = configurations[configIndex - 1];
				for (int i = 0; i < configuration->numInterfaces; i++) {
					UsbInterface* intf = configuration->interfaces[i];

					for (int e = 0; e < intf->numEndpoints; e++) {
						endpoints[numEndpoints] = intf->endpoints[e];
						numEndpoints++;
					}

				}
			}


			UsbBufferDescriptor* descriptor = (UsbBufferDescriptor*) PACKET_MEMORY_BASE;
			int nextBuffer = sizeof (UsbBufferDescriptor) * numEndpoints;
			for (int i = 0; i < numEndpoints; i++) {

				endpoints[i]->bufferDescriptor = descriptor;
				endpoints[i]->epr = &target::USB.EP0R + i;
				endpoints[i]->reset(i);

				int sz = endpoints[i]->txBufferSize;
				descriptor->ADDR_TX = nextBuffer;
				descriptor->COUNT_TX = 0;
				nextBuffer += sz;

				sz = endpoints[i]->rxBufferSize;
				descriptor->ADDR_RX = nextBuffer;
				descriptor->COUNT_RX = (((sz >> 5) - 1) << 10) | (1 << 15);
				nextBuffer += sz;

				descriptor++;
			}

		}

		void handleIrq() {

			if (target::USB.ISTR.getRESET()) {
				target::USB.ISTR.setRESET(0);

				target::USB.DADDR.setADD(0);
				target::USB.DADDR.setEF(1);

				setConfiguration(0);

			}

			if (target::USB.ISTR.getCTR()) {

				int i = target::USB.ISTR.getEP_ID();

				if (i < numEndpoints) {
					endpoints[i]->correctTransfer();
				}

			}

		}

		virtual void sendDeviceDescriptor() {

			unsigned char descriptor[DEVICE_DESCRIPTOR_SIZE];

			descriptor[ 0] = DEVICE_DESCRIPTOR_SIZE; // Size of the Descriptor in Bytes (18 bytes)
			descriptor[ 1] = DEVICE_DESCRIPTOR; // Device Descriptor (0x01)
			descriptor[ 2] = 0x10; // USB Specification Number which device complies too.
			descriptor[ 3] = 0x01; // |
			descriptor[ 4] = 0; // Class Code (Assigned by USB Org)
			descriptor[ 5] = 0; // Subclass Code (Assigned by USB Org)
			descriptor[ 6] = 0; // Protocol Code (Assigned by USB Org)
			descriptor[ 7] = defaultEndpoint.rxBufferSize; // Maximum Packet Size for Zero Endpoint. Valid Sizes are 8; 16; 32; 64
			descriptor[ 8] = idVendor & 0xFF; // Vendor ID (Assigned by USB Org)
			descriptor[ 9] = idVendor >> 8; // |
			descriptor[10] = idProduct & 0xFF; // Product ID (Assigned by Manufacturer)
			descriptor[11] = idProduct >> 8; // |
			descriptor[12] = devRelease & 0xFF; // Device Release Number
			descriptor[13] = devRelease >> 8; // |
			descriptor[14] = strVendor ? STRING_DESCRIPTOR_VENDOR : 0; // Index of Manufacturer String Descriptor
			descriptor[15] = strProduct ? STRING_DESCRIPTOR_PRODUCT : 0; // Index of Product String Descriptor
			descriptor[16] = strProduct ? STRING_DESCRIPTOR_SERIAL_NUMBER : 0; // Index of Serial Number String Descriptor
			descriptor[17] = numConfigurations; // Number of Possible Configurations

			defaultEndpoint.send(descriptor, sizeof (descriptor));
		}

		virtual void sendConfigurationDescriptor(int index, int len) {

			if (index >= 0 && index < numConfigurations) {

				UsbConfiguration* configuration = configurations[index];

				int size = CONFIGURATION_DESCRIPTOR_SIZE;
				for (int i = 0; i < configuration->numInterfaces; i++) {
					UsbInterface* intf = configuration->interfaces[i];
					size += INTERFACE_DESCRIPTOR_SIZE;
					size += intf->getExtraDescriptorSize();
					for (int e = 0; e < intf->numEndpoints; e++) {
						UsbEndpoint* endpoint = intf->endpoints[e];
						if (endpoint->rxBufferSize) {
							size += ENDPOINT_DESCRIPTOR_SIZE;
						}
						if (endpoint->txBufferSize) {
							size += ENDPOINT_DESCRIPTOR_SIZE;
						}
					}

				}

				unsigned char descriptor[size];

				unsigned char* d = descriptor;

				*d++ = CONFIGURATION_DESCRIPTOR_SIZE; // Size of this descriptor in bytes
				*d++ = CONFIGURATION_DESCRIPTOR; // CONFIGURATION descriptor type (= 2)
				*d++ = size & 0xFF; // Total number of bytes in this descriptor and all the following descriptors.
				*d++ = size >> 8; // |
				*d++ = configuration->numInterfaces; // Number of interfaces supported by this configuration
				*d++ = index + 1; // Value used by Set Configuration to select this configuration
				*d++ = 0; // Index of string descriptor describing configuration - set to 0 if no string
				*d++ = 0x80; // D7: Must be set to 1, D6: Self-powered, D5: Remote Wakeup, D4...D0: Set to 0
				*d++ = configuration->maxPower >> 1; // Maximum current drawn by device in this configuration. In units of 2mA. So 50 means 100 mA.

				int epAddress = 1;

				for (int i = 0; i < configuration->numInterfaces; i++) {

					UsbInterface* intf = configuration->interfaces[i];

					int numReportedEndpoints = 0;
					for (int e = 0; e < intf->numEndpoints; e++) {
						UsbEndpoint* endpoint = intf->endpoints[e];
						if (endpoint->rxBufferSize) {
							numReportedEndpoints++;
						}
						if (endpoint->txBufferSize) {
							numReportedEndpoints++;
						}
					}

					*d++ = INTERFACE_DESCRIPTOR_SIZE; // Size of Descriptor in Bytes (9 Bytes)
					*d++ = INTERFACE_DESCRIPTOR; // Interface Descriptor (0x04)
					*d++ = i; // Number of Interface
					*d++ = 0; // Value used to select alternative setting
					*d++ = numReportedEndpoints; // Number of Endpoints used for this interface
					*d++ = intf->interfaceClass; // Class Code (Assigned by USB Org)
					*d++ = intf->interfaceSubClass; // Subclass Code (Assigned by USB Org)
					*d++ = intf->interfaceProtocol; // Protocol Code (Assigned by USB Org)
					*d++ = intf->strDescription ? STRING_DESCRIPTOR_INTERFACE | (index << 3) | i : 0; // Index of String Descriptor Describing this interface

					intf->writeExtraDescriptor(&d);

					for (int e = 0; e < intf->numEndpoints; e++) {

						UsbEndpoint* endpoint = intf->endpoints[e];

						for (int dir = 0; dir < 2; dir++) {

							if ((dir == 0 && endpoint->rxBufferSize) || (dir == 1 && endpoint->txBufferSize)) {

								*d++ = ENDPOINT_DESCRIPTOR_SIZE; // Size of Descriptor in Bytes (7 bytes)
								*d++ = ENDPOINT_DESCRIPTOR; // Endpoint Descriptor (0x05)

								// "Endpoint Address
								//  Bits 0..3b Endpoint Number.
								//  Bits 4..6b Reserved. Set to Zero
								//  Bits 7 Direction 0 = Out, 1 = In (Ignored for Control Endpoints)"
								*d++ = epAddress | (dir << 7);

								// "Bits 0..1 Transfer Type
								//  00 = Control
								//  01 = Isochronous
								//  10 = Bulk
								//  11 = Interrupt
								//  Bits 2..7 are reserved. If Isochronous endpoint,
								//  Bits 3..2 = Synchronisation Type (Iso Mode)
								//  00 = No Synchonisation
								//  01 = Asynchronous
								//  10 = Adaptive
								//  11 = Synchronous
								//  Bits 5..4 = Usage Type (Iso Mode)
								//  00 = Data Endpoint
								//  01 = Feedback Endpoint
								//  10 = Explicit Feedback Data Endpoint
								//  11 = Reserved"
								*d++ = endpoint->epType;

								// Maximum Packet Size this endpoint is capable of sending or receiving
								// |
								int bufSize = dir ? endpoint->txBufferSize : endpoint->rxBufferSize;
								*d++ = bufSize & 0xFF;
								*d++ = bufSize >> 8;

								// Interval for polling endpoint data transfers. Value in frame counts. Ignored for Bulk & Control Endpoints. Isochronous must equal 1 and field may range from 1 to 255 for interrupt endpoints.
								*d++ = 1;

							}

						}

						epAddress++;
					}

				}

				defaultEndpoint.sendLong(descriptor, len);
			}

		}

		virtual void sendStringDescriptor(int index) {

			if (index == 0) {

				unsigned char descriptor[4];

				descriptor[ 0] = sizeof (descriptor); // Size of the Descriptor in Bytes
				descriptor[ 1] = STRING_DESCRIPTOR; // Device Descriptor (0x03)
				descriptor[ 2] = 0x09;
				descriptor[ 3] = 0x04;

				defaultEndpoint.send(descriptor, sizeof (descriptor));
			} else {
				const char* str = NULL;
				if (index == STRING_DESCRIPTOR_VENDOR) {
					str = strVendor;
				} else if (index == STRING_DESCRIPTOR_PRODUCT) {
					str = strProduct;
				} else if (index == STRING_DESCRIPTOR_SERIAL_NUMBER) {
					str = strSerialNo;
				} else if ((index & 0xC0) == STRING_DESCRIPTOR_INTERFACE) {
					int config = (index >> 3) & 7;
					int intf = index & 7;

					str = configurations[config]->interfaces[intf]->strDescription;
				}
				if (str) {
					int strLen = 0;
					while (str[strLen]) strLen++;

					int descrLen = 2 + (strLen << 1);

					unsigned char descriptor[descrLen];

					descriptor[0] = descrLen; // Size of the Descriptor in Bytes
					descriptor[1] = STRING_DESCRIPTOR; // Device Descriptor (0x03)
					for (int c = 0; c < strLen; c++) {
						descriptor[2 + (c << 1)] = str[c];
						descriptor[3 + (c << 1)] = 0;
					}

					defaultEndpoint.sendLong(descriptor, descrLen);
				} else {
					defaultEndpoint.sendZLP();
				}
			}

		}

		virtual void handleNonStandardRequest(SetupPacket* packet, int counter, unsigned char* data, int len) {
			defaultEndpoint.sendZLP();
		}

	};

}

void interruptHandlerUSB() {
	if (usbd::usbDevice) {
		usbd::usbDevice->handleIrq();
	}
}
