namespace usbd {

	const int MAX_INTERFACES = 8;

	class UsbConfiguration {
	public:

		int maxPower = 50;

		UsbInterface* interfaces[MAX_INTERFACES];
		int numInterfaces;

		virtual void init() {
			numInterfaces = 0;
			for (int i = 0; i < MAX_INTERFACES && interfaces[i]; i++) {
				interfaces[i]->init();
				numInterfaces++;
			}
		}

	};

}