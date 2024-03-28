
#include "./protocol_buffers.hpp"

#include <google/protobuf/util/json_util.h>

size_t PbWritePacket(xPacketCommandId CmdId, xPacketRequestId RequestId, void * Buffer, size_t BufferSize, xPbMessage & Message) {
	assert(BufferSize >= PacketHeaderSize);
	auto PayloadPtr     = static_cast<ubyte *>(Buffer) + PacketHeaderSize;
	auto MaxPayloadSize = BufferSize - PacketHeaderSize;
	if (!Message.SerializeToArray(PayloadPtr, MaxPayloadSize)) {
		return 0;
	}
	auto Header       = xPacketHeader();
	Header.PacketSize = PacketHeaderSize + Message.GetCachedSize();
	Header.CommandId  = CmdId;
	Header.RequestId  = RequestId;
	Header.Serialize(Buffer);
	return Header.PacketSize;
}

std::string PbToJson(const xPbMessage & PbMessage) {
	auto options                          = google::protobuf::util::JsonOptions();
	options.add_whitespace                = false;  // 添加空白以增加可读性
	options.always_print_primitive_fields = false;  // 总是打印原始字段

	std::string json_str;
	// 将 MyMessage 对象转换为 JSON 字符串
	auto ConvertStatus = google::protobuf::util::MessageToJsonString(PbMessage, &json_str, options);
	if (!ConvertStatus.ok()) {
		PbMessage.SerializeToString(&json_str);
	}
	return json_str;
}

std::string PbToJsonDetailed(const xPbMessage & PbMessage) {
	auto options                          = google::protobuf::util::JsonOptions();
	options.add_whitespace                = false;  // 添加空白以增加可读性
	options.always_print_primitive_fields = true;   // 总是打印原始字段

	std::string json_str;
	// 将 MyMessage 对象转换为 JSON 字符串
	auto ConvertStatus = google::protobuf::util::MessageToJsonString(PbMessage, &json_str, options);
	if (!ConvertStatus.ok()) {
		PbMessage.SerializeToString(&json_str);
	}
	return json_str;
}
