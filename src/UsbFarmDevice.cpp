//^ FarmDevice
//^ UsbDevice
//^ McuUid

class FarmDeviceEndopoint : public AbstractBulkEndpoint, public DataSink {
    
public:
    
    Transaction transaction;
    
    bool sendZLP = false;
    
    virtual void init() {
        rxBufferSize = 64;
        txBufferSize = 64;
        AbstractBulkEndpoint::init();
    }

    void correctTransferIn() {
        
        int len = 0;
        unsigned char buffer[txBufferSize];
        while (len < sizeof(buffer) && transaction.read(&buffer[len])) {
            len++;
        }
        
        if (len || sendZLP) {
            send(buffer, len);
        }
        
        sendZLP = len == txBufferSize;
        
    }
    
    void correctTransferOut(unsigned char* data, int len) {
        transaction.receive(data, len, this);
        statRx(EP_VALID);
    }
   
    void put(unsigned char* data, int len) {
        correctTransferIn();
    }

};

class FarmDeviceInterface : public UsbInterface {
public:

    FarmDeviceEndopoint endpoint;

    void init() {
        endpoints[0] = &endpoint;
        UsbInterface::init();
    }

};

class FarmDeviceUsbConfiguration : public UsbConfiguration {
public:
    FarmDeviceInterface interface;

    virtual void init() {
        interfaces[0] = &interface;
        maxPower = 100;
        UsbConfiguration::init();
    }
};

class UsbFarmDevice : public UsbDevice, public FarmDevice {
    FarmDeviceUsbConfiguration configuration;
    McuUid mcuUid;
public:

    void initDevice(char const* vendorName, char const* productName) {

        
        mcuUid.init();

        initFarmDevice(vendorName, productName, mcuUid.uid);

        idVendor = 0x0483;
        idProduct = 0xDEFA;

        strVendor = vendorName;
        strProduct = productName;
        strSerialNo = mcuUid.uid;

        configurations[0] = &configuration;

        configuration.interface.endpoint.transaction.setRequestHandler(this);

        init();
    }

};



