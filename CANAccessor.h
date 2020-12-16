/*
 * CANアクセサ
*/
#include <mcp_can.h>
#include <SPI.h>

class CANAccessor{
    private:
        MCP_CAN *can = NULL;
        byte csPin, intPin;

    public:
        CANAccessor(byte csPin, byte intpin);
        int begin(uint8_t mode, uint8_t speedRate, uint8_t clockFreq);
        void setMode(uint8_t mode);
        int send(uint32_t id, uint8_t *data, uint8_t length);
        int recv(uint32_t *id, uint8_t *data, uint8_t *length);
};
