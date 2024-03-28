#include "./protocol_binary.hpp"

size_t xBinaryMessage::Serialize(void * Dst, size_t Size) {
	auto SSize = static_cast<ssize_t>(Size);
	assert(SSize < std::numeric_limits<ssize_t>::max());

	_Writer.Reset(Dst);
	_RemainSize = SSize;
	SerializeMembers();
	if (Steal(_RemainSize) < 0) {
		return 0;
	}
	return _Writer.Offset();
}

size_t xBinaryMessage::Deserialize(const void * Src, size_t Size) {
	auto SSize = static_cast<ssize_t>(Size);
	assert(SSize < std::numeric_limits<ssize_t>::max());

	_Reader.Reset(Src);
	_RemainSize = SSize;
	DeserializeMembers();
	return Steal(_RemainSize) < 0 ? 0 : _Reader.Offset();
}

void xBinaryMessage::SerializeMembers() {
	_RemainSize = -1;
}

void xBinaryMessage::DeserializeMembers() {
	_RemainSize = -1;
}

/****************/
size_t WritePacket(xPacketCommandId CmdId, xPacketRequestId RequestId, void * Buffer, size_t BufferSize, xBinaryMessage & Message) {
	assert(BufferSize >= PacketHeaderSize);
	auto PayloadPtr     = static_cast<ubyte *>(Buffer) + PacketHeaderSize;
	auto MaxPayloadSize = BufferSize - PacketHeaderSize;
	auto PayloadSize    = Message.Serialize(PayloadPtr, MaxPayloadSize);
	if (!PayloadSize) {
		return 0;
	}

	auto Header       = xPacketHeader();
	Header.PacketSize = PacketHeaderSize + PayloadSize;
	Header.CommandId  = CmdId;
	Header.RequestId  = RequestId;
	Header.Serialize(Buffer);
	return Header.PacketSize;
}
