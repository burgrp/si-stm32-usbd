namespace usbd {

	const int MAX_ENDPOINTS = 8;

	class UsbInterface {
	public:
		UsbEndpoint* endpoints[MAX_ENDPOINTS];
		int numEndpoints;

		const char* strDescription;

		int interfaceClass = 0xFF;
		int interfaceSubClass;
		int interfaceProtocol;

		virtual void init() {
			numEndpoints = 0;
			for (int i = 0; i < MAX_ENDPOINTS && endpoints[i]; i++) {
				endpoints[i]->init();
				numEndpoints++;
			}
		}

		virtual int getExtraDescriptorSize() {
			return 0;
		};

		virtual void writeExtraDescriptor(unsigned char** descriptor) {
		};


	};

}