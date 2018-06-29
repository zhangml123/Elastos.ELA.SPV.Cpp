// Copyright (c) 2012-2018 The Elastos Open Source Project
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstring>
#include <BRTransaction.h>
#include <SDK/Common/Log.h>

#include "Transaction.h"
#include "Payload/PayloadCoinBase.h"
#include "Payload/PayloadIssueToken.h"
#include "Payload/PayloadWithDrawAsset.h"
#include "Payload/PayloadRecord.h"
#include "Payload/PayloadRegisterAsset.h"
#include "Payload/PayloadSideMining.h"
#include "Payload/PayloadTransferCrossChainAsset.h"
#include "Payload/PayloadTransferAsset.h"
#include "Payload/PayloadRegisterIdentification.h"
#include "BRCrypto.h"
#include "ELATxOutput.h"
#include "Utils.h"
#include "BRAddress.h"
#include "Wallet.h"

namespace Elastos {
	namespace ElaWallet {

		Transaction::Transaction() :
				_isRegistered(false),
				_manageRaw(true) {
			_transaction = ELATransactionNew();
		}

		Transaction::Transaction(ELATransaction *tx, bool manageRaw) :
				_isRegistered(false),
				_manageRaw(manageRaw) {
			_transaction = tx;
		}

		Transaction::Transaction(const ELATransaction &tx) :
				_isRegistered(false),
				_manageRaw(true) {
			_transaction = ELATransactionCopy(&tx);

		}

		Transaction::Transaction(const Transaction &tx) :
				_manageRaw(true) {
			_isRegistered = tx._isRegistered;
			_transaction = ELATransactionCopy(tx._transaction);
		}

		Transaction &Transaction::operator=(const Transaction &tx) {
			_manageRaw = true;
			_isRegistered = tx._isRegistered;
			_transaction = ELATransactionCopy(tx._transaction);
			return *this;
		}

		Transaction::Transaction(const CMBlock &buffer) :
				_isRegistered(false),
				_manageRaw(true) {
			_transaction = ELATransactionNew();

			ByteStream stream(buffer, buffer.GetSize(), false);
			this->Deserialize(stream);
		}

		Transaction::Transaction(const CMBlock &buffer, uint32_t blockHeight, uint32_t timeStamp) :
				_isRegistered(false),
				_manageRaw(true) {

			_transaction = ELATransactionNew();

			ByteStream stream(buffer, buffer.GetSize(), false);
			this->Deserialize(stream);

			_transaction->raw.blockHeight = blockHeight;
			_transaction->raw.timestamp = timeStamp;
		}

		Transaction::~Transaction() {
			if (_manageRaw && _transaction != nullptr)
				ELATransactionFree(_transaction);
		}

		std::string Transaction::toString() const {
			//todo complete me
			return "";
		}

		BRTransaction *Transaction::getRaw() const {
			return &_transaction->raw;
		}

		bool Transaction::isRegistered() const {
			return _isRegistered;
		}

		bool &Transaction::isRegistered() {
			return _isRegistered;
		}

		void Transaction::resetHash() {
			UInt256Set(&_transaction->raw.txHash, UINT256_ZERO);
		}

		UInt256 Transaction::getHash() const {
			UInt256 emptyHash = UINT256_ZERO;
			if (UInt256Eq(&_transaction->raw.txHash, &emptyHash)) {
				ByteStream ostream;
				serializeUnsigned(ostream);
				CMBlock buff = ostream.getBuffer();
				BRSHA256_2(&_transaction->raw.txHash, buff, buff.GetSize());
			}
			return _transaction->raw.txHash;
		}

		uint32_t Transaction::getVersion() const {
			return _transaction->raw.version;
		}

		void Transaction::setTransactionType(ELATransaction::Type type) {
			if (_transaction->type != type) {
				_transaction->type = type;
				_transaction->payload = newPayload(type);
			}
		}

		ELATransaction::Type Transaction::getTransactionType() const {
			return _transaction->type;
		}

		IPayload *Transaction::newPayload(ELATransaction::Type type) {
			//todo initializing payload other than just creating
			return ELAPayloadNew(type);
		}

		std::vector<std::string> Transaction::getInputAddresses() {

			std::vector<std::string> addresses(_transaction->raw.inCount);
			for (int i = 0; i < _transaction->raw.inCount; i++)
				addresses[i] = _transaction->raw.inputs[i].address;

			return addresses;
		}

		const std::vector<TransactionOutput *> &Transaction::getOutputs() const {
			return _transaction->outputs;
		}

		std::vector<std::string> Transaction::getOutputAddresses() {

			const std::vector<TransactionOutput *> &outputs = getOutputs();
			ssize_t len = outputs.size();
			std::vector<std::string> addresses(len);
			for (int i = 0; i < len; i++)
				addresses[i] = outputs[i]->getAddress();

			return addresses;
		}

		uint32_t Transaction::getLockTime() {

			return _transaction->raw.lockTime;
		}

		void Transaction::setLockTime(uint32_t lockTime) {

			_transaction->raw.lockTime = lockTime;
		}

		uint32_t Transaction::getBlockHeight() {

			return _transaction->raw.blockHeight;
		}

		uint32_t Transaction::getTimestamp() {

			return _transaction->raw.timestamp;
		}

		void Transaction::setTimestamp(uint32_t timestamp) {

			_transaction->raw.timestamp = timestamp;
		}

		void Transaction::addInput(const UInt256 &hash, uint32_t index, uint64_t amount,
								   const CMBlock script, const CMBlock signature, uint32_t sequence) {
			BRTransactionAddInput(&_transaction->raw, hash, index, amount,
								  script, script.GetSize(), signature, signature.GetSize(),
								  sequence);

			Program *program(new Program());
			program->setCode(script);
			program->setParameter(signature);
			addProgram(program);
		}

		void Transaction::addOutput(TransactionOutput *output) {
			_transaction->outputs.push_back(output);
		}

		// shuffles order of tx outputs
		void Transaction::shuffleOutputs() {
			ELATransactionShuffleOutputs(_transaction);
		}

		size_t Transaction::getSize() {
			return ELATransactionSize(_transaction);
		}

		uint64_t Transaction::getStandardFee() {
			return ELATransactionStandardFee(_transaction);
		}

		bool Transaction::isSigned() {
			return ELATransactionIsSign(_transaction);
		}

		bool Transaction::sign(const WrapperList<Key, BRKey> &keys, int forkId) {
			return transactionSign(forkId, keys);
		}

		bool Transaction::sign(const Key &key, int forkId) {

			WrapperList<Key, BRKey> keys(1);
			keys.push_back(key);
			return sign(keys, forkId);
		}

		bool Transaction::transactionSign(int forkId, const WrapperList<Key, BRKey> keys) {
			const int SIGHASH_ALL = 0x01; // default, sign all of the outputs
			size_t i, j, keysCount = keys.size();
			BRAddress addrs[keysCount], address;

			assert(keysCount > 0);
			Log::getLogger()->info("Transaction transactionSign method begin, key counts = {}.", keysCount);

			for (i = 0; i < keysCount; i++) {
				addrs[i] = BR_ADDRESS_NONE;
				std::string tempAddr = keys[i].address();
				if (!tempAddr.empty()) {
					strncpy(addrs[i].s, tempAddr.c_str(), sizeof(BRAddress) - 1);
				}
			}

			Log::getLogger()->info("Transaction transactionSign input sign begin.");
			size_t size = _transaction->raw.inCount;
			for (i = 0; i < size; i++) {
				BRTxInput *input = &_transaction->raw.inputs[i];
				if (i >= _transaction->programs.size()) {
					std::string redeemScript = keys[i].keyToRedeemScript(ELA_STANDARD);
					CMBlock code = Utils::decodeHex(redeemScript);
					Program *program(new Program());
					program->setCode(code);
					_transaction->programs.push_back(program);
				}
				Program *program = _transaction->programs[i];
				if (!BRAddressFromScriptPubKey(address.s, sizeof(address), input->script, input->scriptLen)) continue;
				j = 0;
				while (j < keysCount && !BRAddressEq(&addrs[j], &address)) j++;
				if (j >= keysCount) continue;

				Log::getLogger()->info("Transaction transactionSign begin sign the {} input.", i);
				const uint8_t *elems[BRScriptElements(NULL, 0, program->getCode(), program->getCode().GetSize())];
				size_t elemsCount = BRScriptElements(elems, sizeof(elems) / sizeof(*elems), program->getCode(),
													 program->getCode().GetSize());
				CMBlock pubKey = keys[j].getPubkey();
				size_t pkLen = pubKey.GetSize();
				uint8_t sig[73], script[1 + sizeof(sig) + 1 + pkLen];
				size_t sigLen, scriptLen;
				UInt256 md = UINT256_ZERO;
				ByteStream ostream;
				serializeUnsigned(ostream);
				CMBlock data = ostream.getBuffer();
				if (elemsCount >= 2 && *elems[elemsCount - 2] == OP_EQUALVERIFY) { // pay-to-pubkey-hash
					Log::getLogger()->info("Transaction transactionSign the {} input pay to pubkey hash.", i);

					BRSHA256_2(&md, data, data.GetSize());
					sigLen = BRKeySign(keys[j].getRaw(), sig, sizeof(sig) - 1, md);
					sig[sigLen++] = forkId | SIGHASH_ALL;
					scriptLen = BRScriptPushData(script, sizeof(script), sig, sigLen);
					scriptLen += BRScriptPushData(&script[scriptLen], sizeof(script) - scriptLen, pubKey, pkLen);
					BRTxInputSetSignature(input, script, scriptLen);
				} else { // pay-to-pubkey
					Log::getLogger()->info("Transaction transactionSign the {} input pay to pubkey.", i);

					BRSHA256_2(&md, data, data.GetSize());
					sigLen = BRKeySign(keys[j].getRaw(), sig, sizeof(sig) - 1, md);
					sig[sigLen++] = forkId | SIGHASH_ALL;
					scriptLen = BRScriptPushData(script, sizeof(script), sig, sigLen);
					BRTxInputSetSignature(input, script, scriptLen);
				}

				CMBlock shaData(sizeof(UInt256));
				BRSHA256(shaData, data, data.GetSize());
				CMBlock signData = keys[j].compactSign(shaData);
				program->setParameter(signData);

				Log::getLogger()->info("Transaction transactionSign end sign the {} input.", i);
			}

			return isSigned();
		}

		bool Transaction::isStandard() {
			return BRTransactionIsStandard(&_transaction->raw) != 0;
		}

		UInt256 Transaction::getReverseHash() {

			return UInt256Reverse(&_transaction->raw.txHash);
		}

		uint64_t Transaction::getMinOutputAmount() {

			return TX_MIN_OUTPUT_AMOUNT;
		}

		const IPayload *Transaction::getPayload() const {
			return _transaction->payload;
		}

		IPayload *Transaction::getPayload() {
			return _transaction->payload;
		}

		void Transaction::addAttribute(Attribute *attribute) {
			_transaction->attributes.push_back(attribute);
		}

		const std::vector<Attribute *> &Transaction::getAttributes() const {
			return _transaction->attributes;
		}

		void Transaction::addProgram(Program *program) {
			_transaction->programs.push_back(program);
		}

		const std::vector<Program *> &Transaction::getPrograms() const {
			return _transaction->programs;
		}


		const std::string Transaction::getRemark() const {
			return _transaction->Remark;
		}

		void Transaction::setRemark(const std::string &remark) {
			_transaction->Remark = remark;
		}

		void Transaction::Serialize(ByteStream &ostream) const {
			serializeUnsigned(ostream);

			ostream.putVarUint(_transaction->programs.size());
			for (size_t i = 0; i < _transaction->programs.size(); i++) {
				_transaction->programs[i]->Serialize(ostream);
			}
		}

		void Transaction::serializeUnsigned(ByteStream &ostream) const {
			ostream.put((uint8_t) _transaction->type);

			ostream.put(_transaction->payloadVersion);

			assert(_transaction->payload != nullptr);
			_transaction->payload->Serialize(ostream);

			ostream.putVarUint(_transaction->attributes.size());
			for (size_t i = 0; i < _transaction->attributes.size(); i++) {
				_transaction->attributes[i]->Serialize(ostream);
			}

			ostream.putVarUint(_transaction->raw.inCount);
			for (size_t i = 0; i < _transaction->raw.inCount; i++) {
				uint8_t transactionHashData[256 / 8];
				UInt256Set(transactionHashData, _transaction->raw.inputs[i].txHash);
				ostream.putBytes(transactionHashData, 256 / 8);

				uint8_t indexData[16 / 8];
				UInt16SetLE(indexData, uint16_t(_transaction->raw.inputs[i].index));
				ostream.putBytes(indexData, 16 / 8);

				uint8_t sequenceData[32 / 8];
				UInt32SetLE(sequenceData, _transaction->raw.inputs[i].sequence);
				ostream.putBytes(sequenceData, 32 / 8);
			}

			const std::vector<TransactionOutput *> &outputs = getOutputs();
			ostream.putVarUint(outputs.size());
			for (size_t i = 0; i < outputs.size(); i++) {
				outputs[i]->Serialize(ostream);
			}

			uint8_t lockTimeData[32 / 8];
			UInt32SetLE(lockTimeData, _transaction->raw.lockTime);
			ostream.putBytes(lockTimeData, sizeof(lockTimeData));
		}

		bool Transaction::Deserialize(ByteStream &istream) {
			if (!istream.readBytes(&_transaction->type, 1))
				return false;
			if (!istream.readBytes(&_transaction->payloadVersion, 1))
				return false;

			if (_transaction->payload)
				delete _transaction->payload;

			_transaction->payload = newPayload(_transaction->type);
			if (_transaction->payload == nullptr) {
				Log::getLogger()->error("new payload with type={} when deserialize error", _transaction->type);
				return false;
			}
			if (!_transaction->payload->Deserialize(istream))
				return false;

			uint64_t attributeLength = 0;
			if (!istream.readVarUint(attributeLength))
				return false;

			_transaction->attributes.resize(attributeLength);
			for (size_t i = 0; i < attributeLength; i++) {
				_transaction->attributes[i] = new Attribute();
				if (!_transaction->attributes[i]->Deserialize(istream)) {
					Log::getLogger()->error("deserialize tx attribute[{}] error", i);
				}
			}

			uint64_t inCount = 0;
			if (!istream.readVarUint(inCount)) {
				Log::getLogger()->error("deserialize tx inCount error");
				return false;
			}

			for (size_t i = 0; i < inCount; i++) {
				UInt256 txHash;
				if (!istream.readBytes(txHash.u8, sizeof(txHash))) {
					Log::getLogger()->error("deserialize tx's txHash error");
					return false;
				}

				uint16_t index = 0;
				if (!istream.readBytes(&index, sizeof(index))) {
					Log::getLogger()->error("deserialize tx input[{}] error", i);
					return false;
				}

				uint32_t sequence = 0;
				if (!istream.readBytes(&sequence, sizeof(sequence))) {
					Log::getLogger()->error("deserialize tx sequence error");
					return false;
				}

				BRTransactionAddInput(&_transaction->raw, txHash, index, 0, nullptr, 0, nullptr, 0, sequence);
			}

			uint64_t outputLength = 0;
			if (!istream.readVarUint(outputLength)) {
				Log::getLogger()->error("deserialize tx output len error");
				return false;
			}

			// TODO possible memory leak
			_transaction->outputs.resize(outputLength);
			for (size_t i = 0; i < outputLength; i++) {
				_transaction->outputs[i] = new TransactionOutput();
				_transaction->outputs[i]->Deserialize(istream);
			}

			uint8_t lockTimeData[32 / 8];
			istream.getBytes(lockTimeData, sizeof(lockTimeData));
			_transaction->raw.lockTime = UInt32GetLE(lockTimeData);

			uint64_t programLength = istream.getVarUint();
			_transaction->programs.resize(programLength);
			for (size_t i = 0; i < programLength; i++) {
				_transaction->programs[i] = new Program();
				_transaction->programs[i]->Deserialize(istream);
			}

			ByteStream ostream;
			serializeUnsigned(ostream);
			CMBlock buff = ostream.getBuffer();
			BRSHA256_2(&_transaction->raw.txHash, buff, buff.GetSize());

			return true;
		}

		nlohmann::json Transaction::toJson() const {
			nlohmann::json jsonData;

			jsonData["IsRegistered"] = _isRegistered;

			jsonData["TxHash"] = Utils::UInt256ToString(getHash());
			jsonData["Version"] = _transaction->raw.version;
			jsonData["LockTime"] = _transaction->raw.lockTime;
			jsonData["BlockHeight"] = _transaction->raw.blockHeight;
			jsonData["Timestamp"] = _transaction->raw.timestamp;

			std::vector<nlohmann::json> inputs(_transaction->raw.inCount);
			for (size_t i = 0; i < _transaction->raw.inCount; ++i) {
				BRTxInput *input = &_transaction->raw.inputs[i];
				nlohmann::json jsonData;

				jsonData["TxHash"] = Utils::UInt256ToString(input->txHash);
				jsonData["Index"] = input->index;
				jsonData["Address"] = std::string(input->address);
				jsonData["Amount"] = input->amount;
				jsonData["Script"] = Utils::encodeHex(input->script, input->scriptLen);
				jsonData["Signature"] = Utils::encodeHex(input->signature, input->sigLen);
				jsonData["Sequence"] = input->sequence;

				inputs[i] = jsonData;
			}
			jsonData["Inputs"] = inputs;

			jsonData["Type"] = (uint8_t) _transaction->type;
			jsonData["PayloadVersion"] = _transaction->payloadVersion;
			jsonData["PayLoad"] = _transaction->payload->toJson();

			std::vector<nlohmann::json> attributes(_transaction->attributes.size());
			for (size_t i = 0; i < attributes.size(); ++i) {
				attributes[i] = _transaction->attributes[i]->toJson();
			}
			jsonData["Attributes"] = attributes;

			std::vector<nlohmann::json> programs(_transaction->programs.size());
			for (size_t i = 0; i < programs.size(); ++i) {
				programs[i] = _transaction->programs[i]->toJson();
			}
			jsonData["Programs"] = programs;

			const std::vector<TransactionOutput *> &txOutputs = getOutputs();
			std::vector<nlohmann::json> outputs(txOutputs.size());
			for (size_t i = 0; i < txOutputs.size(); ++i) {
				outputs[i] = txOutputs[i]->toJson();
			}
			jsonData["Outputs"] = outputs;

			jsonData["Fee"] = _transaction->fee;

			jsonData["Remark"] = _transaction->Remark;

			return jsonData;
		}

		void Transaction::fromJson(const nlohmann::json &jsonData) {
			_isRegistered = jsonData["IsRegistered"];

			_transaction->raw.txHash = Utils::UInt256FromString(jsonData["TxHash"].get<std::string>());
			_transaction->raw.version = jsonData["Version"].get<uint32_t>();
			_transaction->raw.lockTime = jsonData["LockTime"].get<uint32_t>();
			_transaction->raw.blockHeight = jsonData["BlockHeight"].get<uint32_t>();
			_transaction->raw.timestamp = jsonData["Timestamp"].get<uint32_t>();

			std::vector<nlohmann::json> inputs = jsonData["Inputs"];
			_transaction->raw.inCount = inputs.size();

			for (size_t i = 0; i < _transaction->raw.inCount; ++i) {
				nlohmann::json jsonData = inputs[i];

				std::string address = jsonData["Address"].get<std::string>();
				UInt256 txHash = Utils::UInt256FromString(jsonData["TxHash"].get<std::string>());
				uint32_t index = jsonData["Index"].get<uint32_t>();
				uint64_t amount = jsonData["Amount"].get<uint64_t>();
				CMBlock script = Utils::decodeHex(jsonData["Script"].get<std::string>());
				CMBlock signature = Utils::decodeHex(jsonData["Signature"].get<std::string>());
				uint32_t sequence = jsonData["Sequence"].get<uint32_t>();

				BRTransactionAddInput(&_transaction->raw, txHash, index, amount, script, script.GetSize(), signature,
									  signature.GetSize(), sequence);
				assert(0 == strcmp(address.c_str(), _transaction->raw.inputs[i].address));
			}

			_transaction->type = ELATransaction::Type(jsonData["Type"].get<uint8_t>());
			_transaction->payloadVersion = jsonData["PayloadVersion"];

			_transaction->payload = newPayload(_transaction->type);
			if (_transaction->payload == nullptr) {
				Log::getLogger()->error("payload is nullptr when convert from json");
			} else {
				_transaction->payload->fromJson(jsonData["PayLoad"]);
			}

			// TODO possible memory leak
			std::vector<nlohmann::json> attributes = jsonData["Attributes"];
			_transaction->attributes.resize(attributes.size());
			for (size_t i = 0; i < _transaction->attributes.size(); ++i) {
				_transaction->attributes[i] = new Attribute();
				_transaction->attributes[i]->fromJson(attributes[i]);
			}

			std::vector<nlohmann::json> programs = jsonData["Programs"];
			_transaction->programs.resize(programs.size());
			for (size_t i = 0; i < _transaction->programs.size(); ++i) {
				_transaction->programs[i] = new Program();
				_transaction->programs[i]->fromJson(programs[i]);
			}

			std::vector<nlohmann::json> outputs = jsonData["Outputs"];
			_transaction->outputs.resize(outputs.size());
			for (size_t i = 0; i < outputs.size(); ++i) {
				_transaction->outputs[i] = new TransactionOutput();
				_transaction->outputs[i]->fromJson(outputs[i]);
			}

			_transaction->fee = jsonData["Fee"].get<uint64_t>();

			_transaction->Remark = jsonData["Remark"].get<std::string>();
		}

		uint64_t Transaction::calculateFee(uint64_t feePerKb) {
			uint64_t size = ELATransactionSize(_transaction);
			return  ((size + 999) / 1000) * feePerKb;
		}

		void
		Transaction::generateExtraTransactionInfo(nlohmann::json &rawTxJson, const boost::shared_ptr<Wallet> &wallet) {

			std::string remark = wallet->GetRemark(Utils::UInt256ToString(getHash()));
			setRemark(remark);

			nlohmann::json summary;
			summary["Status"] = getStatus(wallet);
			summary["ConfirmStatus"] = getConfirmInfo(wallet);
			summary["Amount"] = _transaction->outputs.empty() ? 0 : _transaction->outputs[0]->getAmount();
			std::string toAddress = _transaction->outputs.empty() ? "" : _transaction->outputs[0]->getAddress();
			summary["Type"] = wallet->containsAddress(toAddress) ? "Incoming" : "Outcoming";
			summary["ToAddress"] = toAddress;
			summary["Remark"] = getRemark();

			rawTxJson["Summary"] = summary;
		}

		std::string Transaction::getConfirmInfo(const boost::shared_ptr<Wallet> &wallet) {
			if(getBlockHeight() == TX_UNCONFIRMED)
				return std::to_string(0);

			uint32_t confirmCount = wallet->getBlockHeight() - getBlockHeight();
			return confirmCount <= 6 ? std::to_string(confirmCount) : "6+";
		}

		std::string Transaction::getStatus(const boost::shared_ptr<Wallet> &wallet) {
			if(getBlockHeight() == TX_UNCONFIRMED)
				return "Unconfirmed";

			uint32_t confirmCount = wallet->getBlockHeight() - getBlockHeight();
			return confirmCount <= 6 ? "Pending" : "Confirmed";
		}
	}
}
