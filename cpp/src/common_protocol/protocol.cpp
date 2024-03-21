#include "./protocol.hpp"

#include <google/protobuf/util/json_util.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>

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

/* Protocols */

std::vector<ubyte> Encrypt(const void * Data, size_t DataSize, const std::string & AesKey) {
	auto Blocks        = (DataSize + 16) / 16;
	auto Length        = Blocks * 16;
	auto R             = std::vector<ubyte>(Length * 2);
	auto Dst           = R.data();
	auto Src           = Dst + Length;
	auto Padding       = Src + DataSize;
	auto PaddingLength = Length - DataSize;
	memcpy(Src, Data, DataSize);
	for (size_t i = 0; i < PaddingLength; ++i) {
		*Padding++ = (uint8_t)PaddingLength;
	}
	mbedtls_aes_context aes_ctx;
	mbedtls_aes_init(&aes_ctx);
	mbedtls_aes_setkey_enc(&aes_ctx, (const ubyte *)AesKey.data(), 128);
	auto G = xScopeGuard([&] { mbedtls_aes_free(&aes_ctx); });

	for (size_t i = 0; i < Blocks; ++i) {
		mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, Src, Dst);
		Src += 16;
		Dst += 16;
	}

	R.resize(Length);
	return R;
}

std::vector<ubyte> Decrypt(const void * Data, size_t DataSize, const std::string & AesKey) {
	if (DataSize % 16) {
		return {};
	}

	mbedtls_aes_context aes_ctx;
	mbedtls_aes_init(&aes_ctx);
	mbedtls_aes_setkey_dec(&aes_ctx, (const ubyte *)AesKey.data(), 128);
	auto G = xScopeGuard([&] { mbedtls_aes_free(&aes_ctx); });

	auto R      = std::vector<ubyte>(DataSize);
	auto Blocks = DataSize / 16;
	auto Src    = (const ubyte *)Data;
	auto Dst    = R.data();
	for (size_t i = 0; i < Blocks; ++i) {
		mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, Src, Dst);
		Src += 16;
		Dst += 16;
	}
	auto padding = (int)(R[DataSize - 1]);
	R.resize(DataSize - padding);
	return R;
}

std::string EncodeBase64Key(std::string & Key) {
	auto   Encrypted       = std::string(Key.length() * 2, '\0');
	size_t EncryptedLength = 0;
	RuntimeAssert(!mbedtls_base64_encode((ubyte *)Encrypted.data(), Encrypted.size(), &EncryptedLength, (const ubyte *)Key.data(), Key.length()));
	Encrypted.resize(EncryptedLength);
	return Encrypted;
}

std::string DecodeBase64Key(std::string & Key) {
	auto   Result    = std::string(Key.length(), '\0');
	size_t KeyLength = 0;
	RuntimeAssert(!mbedtls_base64_decode((ubyte *)Result.data(), Result.size(), &KeyLength, (const ubyte *)Key.data(), Key.length()));
	Result.resize(KeyLength);
	return Result;
}
