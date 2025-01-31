/**
 * @file SoundData.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SoundData.h"
#include "core/external/openal.h"
#include <sndfile.h>

namespace owl::sound::openal {

enum struct SoundDataType : uint8_t {
	Unsupported,
	Float,
	Int16,
	Ima4,
	MsAdpcm,
};

namespace {
auto sfFormatConvert(const int iSfFormat) -> SoundDataType {
	switch (iSfFormat & SF_FORMAT_SUBMASK) {
		case SF_FORMAT_PCM_24:
		case SF_FORMAT_PCM_32:
		case SF_FORMAT_FLOAT:
		case SF_FORMAT_DOUBLE:
		case SF_FORMAT_VORBIS:
		case SF_FORMAT_OPUS:
		case SF_FORMAT_ALAC_20:
		case SF_FORMAT_ALAC_24:
		case SF_FORMAT_ALAC_32:
		case 0x0080 /*SF_FORMAT_MPEG_LAYER_I*/:
		case 0x0081 /*SF_FORMAT_MPEG_LAYER_II*/:
		case 0x0082 /*SF_FORMAT_MPEG_LAYER_III*/:
			if (alIsExtensionPresent("AL_EXT_FLOAT32") == AL_TRUE)
				return SoundDataType::Float;
			return SoundDataType::Unsupported;
		case SF_FORMAT_IMA_ADPCM:
			if ((iSfFormat & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAV && alIsExtensionPresent("AL_EXT_IMA4") == AL_TRUE &&
				alIsExtensionPresent("AL_SOFT_block_alignment") == AL_TRUE)
				return SoundDataType::Ima4;
			return SoundDataType::Unsupported;
		case SF_FORMAT_MS_ADPCM:
			if ((iSfFormat & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAV &&
				alIsExtensionPresent("AL_SOFT_MSADPCM") == AL_TRUE &&
				alIsExtensionPresent("AL_SOFT_block_alignment") == AL_TRUE)
				return SoundDataType::MsAdpcm;
			return SoundDataType::Unsupported;
		default:
			return SoundDataType::Int16;
	}
}

auto alFormatName(const ALenum iFormat) -> std::string {
	switch (iFormat) {
		case AL_FORMAT_MONO8:
			return "mono8";
		case AL_FORMAT_MONO16:
			return "mono16";
		case AL_FORMAT_STEREO8:
			return "stereo8";
		case AL_FORMAT_STEREO16:
			return "stereo16";
		case AL_FORMAT_MONO_FLOAT32:
			return "monoFloat32";
		case AL_FORMAT_STEREO_FLOAT32:
			return "stereoFloat32";
		case AL_FORMAT_MONO_IMA4:
			return "monoIma4";
		case AL_FORMAT_STEREO_IMA4:
			return "stereoIma4";
		case AL_FORMAT_MONO_MSADPCM_SOFT:
			return "monoMSADPCM";
		case AL_FORMAT_STEREO_MSADPCM_SOFT:
			return "stereoMSADPCM";
		case AL_FORMAT_BFORMAT2D_16:
			return "bf2D16";
		case AL_FORMAT_BFORMAT2D_FLOAT32:
			return "bf2D32";
		case AL_FORMAT_BFORMAT3D_16:
			return "bf3D16";
		case AL_FORMAT_BFORMAT3D_FLOAT32:
			return "bf3D32";
		default:
			return "unknown";
	}
}
}// namespace

SoundData::SoundData(const Specification& iSpecifications) : sound::SoundData{iSpecifications} {
	if (!exists(m_specification.file))
		return;
	// load sound
	SF_INFO sfInfo;
	auto* const file =
			sf_open(reinterpret_cast<const char*>(m_specification.file.u8string().c_str()), SFM_READ, &sfInfo);
	if (file == nullptr) {
		OWL_CORE_WARN("SoundData: Failed to open file '{}'", m_specification.file.string())
		return;
	}
	if (sfInfo.frames < 1) {
		OWL_CORE_WARN("SoundData: File '{}' is empty", m_specification.file.string())
		sf_close(file);
		return;
	}
	auto sampleFormat = sfFormatConvert(sfInfo.format);
	if (sampleFormat == SoundDataType::Unsupported) {
		OWL_CORE_WARN("SoundData: unable to load, Unsupported format...")
		sf_close(file);
		return;
	}
	// Compute block align
	ALint byteblockalign = 0;
	ALint splblockalign = 0;
	if (sampleFormat == SoundDataType::Ima4 || sampleFormat == SoundDataType::MsAdpcm) {
		if (sfInfo.channels > 2) {
			OWL_CORE_WARN("SoundData: unable to load {} format only supports 2 chanel max...",
						  magic_enum::enum_name(sampleFormat))
			sf_close(file);
			return;
		}

		SF_CHUNK_INFO inf = {"fmt ", 4, 0, nullptr};

		/* If there's an issue getting the chunk or block alignment, load as
		 * 16-bit and have libsndfile do the conversion.
		 */
		if (const SF_CHUNK_ITERATOR* iter = sf_get_chunk_iterator(file, &inf);
			iter == nullptr || sf_get_chunk_size(iter, &inf) != SF_ERR_NO_ERROR || inf.datalen < 14)
			sampleFormat = SoundDataType::Int16;
		else {
			std::vector<ALubyte> buffer(inf.datalen);
			inf.data = buffer.data();
			if (sf_get_chunk_data(iter, &inf) != SF_ERR_NO_ERROR)
				sampleFormat = SoundDataType::Int16;
			else {
				/* Read the nBlockAlign field, and convert from bytes- to
				 * samples-per-block (verifying it's valid by converting back
				 * and comparing to the original value).
				 */
				byteblockalign = buffer[12] | buffer[13] << 8;
				if (sampleFormat == SoundDataType::Ima4) {
					splblockalign = (byteblockalign / sfInfo.channels - 4) / 4 * 8 + 1;
					if (splblockalign < 1 || ((splblockalign - 1) / 2 + 4) * sfInfo.channels != byteblockalign)
						sampleFormat = SoundDataType::Int16;
				} else {
					splblockalign = (byteblockalign / sfInfo.channels - 7) * 2 + 2;
					if (splblockalign < 2 || ((splblockalign - 2) / 2 + 7) * sfInfo.channels != byteblockalign)
						sampleFormat = SoundDataType::Int16;
				}
			}
		}
	}
	if (sampleFormat == SoundDataType::Int16) {
		splblockalign = 1;
		byteblockalign = sfInfo.channels * 2;
	} else if (sampleFormat == SoundDataType::Float) {
		splblockalign = 1;
		byteblockalign = sfInfo.channels * 4;
	}

	// Determine OpenAL Format
	ALenum format = AL_NONE;
	if (sfInfo.channels == 1) {
		if (sampleFormat == SoundDataType::Int16)
			format = AL_FORMAT_MONO16;
		else if (sampleFormat == SoundDataType::Float)
			format = AL_FORMAT_MONO_FLOAT32;
		else if (sampleFormat == SoundDataType::Ima4)
			format = AL_FORMAT_MONO_IMA4;
		else if (sampleFormat == SoundDataType::MsAdpcm)
			format = AL_FORMAT_MONO_MSADPCM_SOFT;
	} else if (sfInfo.channels == 2) {
		if (sampleFormat == SoundDataType::Int16)
			format = AL_FORMAT_STEREO16;
		else if (sampleFormat == SoundDataType::Float)
			format = AL_FORMAT_STEREO_FLOAT32;
		else if (sampleFormat == SoundDataType::Ima4)
			format = AL_FORMAT_STEREO_IMA4;
		else if (sampleFormat == SoundDataType::MsAdpcm)
			format = AL_FORMAT_STEREO_MSADPCM_SOFT;
	} else if (sfInfo.channels == 3) {
		if (sf_command(file, SFC_WAVEX_GET_AMBISONIC, nullptr, 0) == SF_AMBISONIC_B_FORMAT) {
			if (sampleFormat == SoundDataType::Int16)
				format = AL_FORMAT_BFORMAT2D_16;
			else if (sampleFormat == SoundDataType::Float)
				format = AL_FORMAT_BFORMAT2D_FLOAT32;
		}
	} else if (sfInfo.channels == 4) {
		if (sf_command(file, SFC_WAVEX_GET_AMBISONIC, nullptr, 0) == SF_AMBISONIC_B_FORMAT) {
			if (sampleFormat == SoundDataType::Int16)
				format = AL_FORMAT_BFORMAT3D_16;
			else if (sampleFormat == SoundDataType::Float)
				format = AL_FORMAT_BFORMAT3D_FLOAT32;
		}
	}

	if (format == AL_NONE) {
		OWL_CORE_WARN("SoundData: Unsupported OpenAL format.")
		sf_close(file);
		return;
	}
	if (sfInfo.frames / splblockalign > static_cast<sf_count_t>(INT_MAX / byteblockalign)) {
		OWL_CORE_WARN("SoundData: Too many samples.")
		sf_close(file);
		return;
	}

	/* Decode the whole audio file to a buffer. */
	std::vector<char> buffer(static_cast<size_t>(sfInfo.frames / splblockalign * byteblockalign));

	sf_count_t numFrames = 0;
	if (sampleFormat == SoundDataType::Int16)
		numFrames = sf_readf_short(file, reinterpret_cast<int16_t*>(buffer.data()), sfInfo.frames);
	else if (sampleFormat == SoundDataType::Float)
		numFrames = sf_readf_float(file, reinterpret_cast<float*>(buffer.data()), sfInfo.frames);
	else {
		const sf_count_t count = sfInfo.frames / splblockalign * byteblockalign;
		numFrames = sf_read_raw(file, buffer.data(), count);
		if (numFrames > 0)
			numFrames = numFrames / byteblockalign * splblockalign;
	}
	if (numFrames < 1) {
		sf_close(file);
		OWL_CORE_WARN("SoundData: Failed to read samples.")
		return;
	}
	const auto numBytes = static_cast<ALsizei>(numFrames / splblockalign * byteblockalign);

	OWL_CORE_INFO("SoundData: loading {} ({}, {}Hz).", m_specification.file.string(), alFormatName(format),
				  sfInfo.samplerate)

	/* Buffer the audio data into a new buffer object, then free the data and
	 * close the file.
	 */
	m_buffer = 0;
	alGenBuffers(1, &m_buffer);
	if (splblockalign > 1)
		alBufferi(m_buffer, AL_UNPACK_BLOCK_ALIGNMENT_SOFT, splblockalign);
	alBufferData(m_buffer, format, buffer.data(), numBytes, sfInfo.samplerate);

	sf_close(file);

	/* Check if an error occurred, and clean up if so. */
	if (const ALenum err = alGetError(); err != AL_NO_ERROR) {
		OWL_CORE_ERROR("SoundData: OpenAL error: {}.", alGetString(err))
		if (m_buffer != 0u && alIsBuffer(m_buffer) == AL_TRUE)
			alDeleteBuffers(1, &m_buffer);
		return;
	}
}

SoundData::~SoundData() {
	if (m_buffer != 0u && alIsBuffer(m_buffer) == AL_TRUE)
		alDeleteBuffers(1, &m_buffer);
}


}// namespace owl::sound::openal
