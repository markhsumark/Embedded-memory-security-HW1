/*
 * Copyright (c) 2017 Jason Lowe-Power
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mark_ecc/ecc_memobj.hh"

#include "base/trace.hh"
#include "debug/EccMemobj.hh"
#include "mem/packet.hh"

namespace gem5
{

EccMemobj::EccMemobj(const EccMemobjParams &params) :
    SimObject(params),
    instPort(params.name + ".inst_port", this),
    dataPort(params.name + ".data_port", this),
    memPort(params.name + ".mem_side", this),
    blocked(false)
{
}

Port &
EccMemobj::getPort(const std::string &if_name, PortID idx)
{
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    // This is the name from the Python SimObject declaration (EccMemobj.py)
    if (if_name == "mem_side") {
        return memPort;
    } else if (if_name == "inst_port") {
        return instPort;
    } else if (if_name == "data_port") {
        return dataPort;
    } else {
        // pass it along to our super class
        return SimObject::getPort(if_name, idx);
    }
}

void
EccMemobj::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}

AddrRangeList
EccMemobj::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
EccMemobj::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(EccMemobj, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

void
EccMemobj::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleFunctional(pkt);
}

bool
EccMemobj::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // Just forward to the memobj.
    if (!owner->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
EccMemobj::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

void
EccMemobj::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
EccMemobj::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleResponse(pkt);
}

void
EccMemobj::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

void
EccMemobj::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

bool
EccMemobj::handleRequest(PacketPtr pkt)
{
    if (blocked) {
        // There is currently an outstanding request. Stall.
        return false;
    }

    // DPRINTF(EccMemobj, "Got request for addr %#x\n", pkt->getAddr());
    // todo
    if(pkt->isWrite()){
        uint64_t addr = pkt->getAddr();
        uint64_t id = pkt->id;
        DPRINTF(EccMemobj, "handleRequest pkt id is %llu\n", id);
        DPRINTF(EccMemobj, "handleRequest pkt addr is %llu\n", addr);
        uint8_t* data = pkt->getPtr<uint8_t>();
        unsigned size = pkt->getSize();
        // DPRINTF(EccMemobj, "handleRequest pkt data is \n");
        // for(size_t i= 0; i<size; i++ ){
        //     DPRINTF(EccMemobj, "%u\n", data[i]);
        // }
        //hamming encode
        ecc_obj.hammingEncode(addr, data, size);
        // DPRINTF(EccMemobj, "parity bits of data is %u\n", ecc_obj.ecc_map[addr]);
        //隨便打亂
        ecc_obj.doError(data, size);
    }
    
    // This memobj is now blocked waiting for the response to this packet.
    blocked = true;

    // Simply forward to the memory port
    memPort.sendPacket(pkt);

    return true;
}

bool
EccMemobj::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    // DPRINTF(EccMemobj, "Got response for addr %#x\n", pkt->getAddr());

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    blocked = false;

    // todo
    if (pkt->isRead()) 
    {
        
        uint64_t addr = pkt->getAddr();
        uint64_t id = pkt->id;
        
        if(ecc_obj.ecc_map.find(addr) != ecc_obj.ecc_map.end()){
            DPRINTF(EccMemobj, "handleResponse pkt id is %llu\n", id);
            DPRINTF(EccMemobj, "handleResponse pkt addr is %llu\n", addr);
            // uint64_t id = pkt->getAddr();
            uint8_t* data = pkt->getPtr<uint8_t>();
            unsigned size = pkt->getSize();
            DPRINTF(EccMemobj, "handleResponse pkt data is \n");
            for(size_t i= 0; i<size; i++ ){
                DPRINTF(EccMemobj, "%u\n", data[i]);
            }
            DPRINTF(EccMemobj, "parity bits of data is %u\n", ecc_obj.ecc_map[addr]);
            //hamming decode
            ecc_obj.hammingDecode(addr, data, size);
        }
    }

    // Simply forward to the memory port
    if (pkt->req->isInstFetch()) {
        instPort.sendPacket(pkt);
    } else {
        dataPort.sendPacket(pkt);
    }

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory object may be unblocked now.
    instPort.trySendRetry();
    dataPort.trySendRetry();

    return true;
}

void
EccMemobj::handleFunctional(PacketPtr pkt)
{
    // Just pass this on to the memory side to handle for now.
    memPort.sendFunctional(pkt);
}

AddrRangeList
EccMemobj::getAddrRanges() const
{
    DPRINTF(EccMemobj, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
EccMemobj::sendRangeChange()
{
    instPort.sendRangeChange();
    dataPort.sendRangeChange();
}
// todo: 這裡要放入ecc的functions實作

int
EccMemobj::EccObj::findMinR(int m) {
    int r = 0;
    while (pow(2, r) < m + r + 1) {
        r++;
    }
    return r;
}


char* 
EccMemobj::EccObj::dataToBinary(uint8_t* data, unsigned size){
    // todo
    // 為二進制字符串分配內存
    int binLength = calBitsLen(size);
    char *binaryString = (char *)malloc((binLength + 1) * sizeof(char));
    if (binaryString == NULL) {
        // printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // 轉換為二進制字符串
    for(size_t x = 0; x<size; x++){
        uint8_t* n = data+x;
        for (int i = 0; i < 8; i++) {
            binaryString[binLength - 1 - 8*(size-x-1) - i] = ((*n >> i) & 1) ? '1' : '0';
        }
    }
    binaryString[binLength] = '\0'; // 添加字符串結束符
    // for(int i=0; i<strlen(binaryString); i++){
    //     printf("%c", binaryString[i]);
    // }
    // printf("\n");
    return binaryString;
}

void
EccMemobj::EccObj::hammingEncode(uint64_t id, uint8_t* data, unsigned size){
    int databits_len = calBitsLen(size);
    uint8_t parityBits = 0;

    int bitPosition = 1;
    int dataIndex = 0;

    char* binaryOfdata = dataToBinary(data, size);
    
    while (dataIndex < databits_len) {
        if ((bitPosition & (bitPosition - 1)) != 0) { // 不是2的幂次方，是数据位
            if(binaryOfdata[dataIndex] == '1'){
                parityBits ^= bitPosition;
            }
            dataIndex++;
        }
        bitPosition++;
    }

    // 存入map中
    ecc_map[id] = parityBits;
    printf("encode parityBits is %u\n", parityBits);

    return;
}

void
EccMemobj::EccObj::hammingDecode(uint64_t id, uint8_t* data, unsigned size){
    uint8_t parityBits = ecc_map[id];
    int databits_len = calBitsLen(size);
    int hamming_len = findMinR(databits_len);

    char* binaryOfdata = dataToBinary(data, size);

    uint8_t checkBits = 0;

    int bitPosition = 1;
    int dataIndex = 0;
    

    while (dataIndex < databits_len) {
        if ((bitPosition & (bitPosition - 1)) != 0) { // 不是2的幂次方，是数据位
            if(binaryOfdata[dataIndex] == '1'){
                checkBits ^= bitPosition;
            }
            dataIndex++;
        }
        bitPosition++;
    }
    int hammingBitsIndex = 1;
    while(hammingBitsIndex <= hamming_len){
        if( (parityBits & uint8_t(pow(2, hamming_len - hammingBitsIndex))) != 0){
            checkBits^= uint8_t(pow(2, hamming_len - hammingBitsIndex));
        }
        hammingBitsIndex++;
    }
    

    // 修復
    if(checkBits != 0){
        printf("error bit occur, checkBits is %d\n", checkBits);
        double result = sqrt(checkBits) +1;
        int flip_bit_i = checkBits- (int)result -1;
        int block_i = flip_bit_i/8;
        int index = flip_bit_i%8;
        uint8_t fix_number = uint8_t( pow(2, 8-index-1) );
        *(data+block_i) ^= fix_number;
        printf("correct data[%d]'s %d bit\n", block_i, 8-index);
        printf("after fix: \n");
        for(size_t i= 0; i<size; i++ )
            printf("%u ", data[i]);
        printf("\n");
    }

    return;
}

int 
EccMemobj::EccObj::calBitsLen(unsigned size){
    return size * sizeof(uint8_t) * 8;
}

void 
EccMemobj::EccObj::doError(uint8_t* data, unsigned size){
    srand(time(NULL));

    int errorProbability = rand() % 100; // 生成0到99之间的随机整数

    if(errorProbability == 0){
        int errorBlock = rand() % size;
        int errorBit = rand() % 8;
        printf("LETS MAKE A ERROR BIT!!!!!!(flip the data[%d]'s %d bit)\n", errorBlock, errorBit);
        data[errorBlock] ^= (1 << errorBit);
    }
    return;
}




} // namespace gem5

