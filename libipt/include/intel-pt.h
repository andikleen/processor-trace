/*
 * Copyright (c) 2013-2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __INTEL_PT_H__
#define __INTEL_PT_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/* Intel(R) Processor Trace (Intel PT) decoder library.
 *
 * This file is logically structured into the following sections:
 *
 * - Version
 * - Opcodes
 * - Errors
 * - Configuration
 * - Packet encoder / decoder
 * - Query decoder
 * - Traced image
 * - Instruction flow decoder
 */



struct pt_encoder;
struct pt_packet_decoder;
struct pt_query_decoder;
struct pt_insn_decoder;



/* A macro to mark functions as exported. */
#ifndef pt_export
#  if defined(__GNUC__)
#    define pt_export __attribute__((visibility("default")))
#  elif defined(_MSC_VER)
#    define pt_export __declspec(dllimport)
#  else
#    error "unknown compiler"
#  endif
#endif



/* Version. */



/** The library version. */
struct pt_version {
	/** Major version number. */
	uint8_t major;

	/** Minor version number. */
	uint8_t minor;

	/** Reserved bits. */
	uint16_t reserved;

	/** Build number. */
	uint32_t build;

	/** Version extension. */
	const char *ext;
};


/** Return the library version. */
extern pt_export struct pt_version pt_library_version();



/* Opcodes. */



/** A one byte opcode. */
enum pt_opcode {
	pt_opc_pad		= 0x00,
	pt_opc_ext		= 0x02,
	pt_opc_psb		= pt_opc_ext,
	pt_opc_tip		= 0x0d,
	pt_opc_tnt_8		= 0x00,
	pt_opc_tip_pge		= 0x11,
	pt_opc_tip_pgd		= 0x01,
	pt_opc_fup		= 0x1d,
	pt_opc_mode		= 0x99,
	pt_opc_tsc		= 0x19,

	/* A free opcode to trigger a decode fault. */
	pt_opc_bad		= 0x59
};

/** A one byte extension code for ext opcodes. */
enum pt_ext_code {
	pt_ext_psb		= 0x82,
	pt_ext_tnt_64		= 0xa3,
	pt_ext_pip		= 0x43,
	pt_ext_ovf		= 0xf3,
	pt_ext_psbend		= 0x23,
	pt_ext_cbr		= 0x03,

	pt_ext_bad		= 0x04
};

/** A one byte opcode mask. */
enum pt_opcode_mask {
	pt_opm_tip		= 0x1f,
	pt_opm_tnt_8		= 0x01,
	pt_opm_tnt_8_shr	= 1,
	pt_opm_fup		= pt_opm_tip,

	/* The bit mask for the compression bits in the opcode. */
	pt_opm_ipc		= 0xe0,

	/* The shift right value for ipc bits. */
	pt_opm_ipc_shr		= 5,

	/* The bit mask for the compression bits after shifting. */
	pt_opm_ipc_shr_mask	= 0x7
};

/** The size of the various opcodes in bytes. */
enum pt_opcode_size {
	pt_opcs_pad		= 1,
	pt_opcs_tip		= 1,
	pt_opcs_tip_pge		= 1,
	pt_opcs_tip_pgd		= 1,
	pt_opcs_fup		= 1,
	pt_opcs_tnt_8		= 1,
	pt_opcs_mode		= 1,
	pt_opcs_tsc		= 1,
	pt_opcs_psb		= 2,
	pt_opcs_psbend		= 2,
	pt_opcs_ovf		= 2,
	pt_opcs_pip		= 2,
	pt_opcs_tnt_64		= 2,
	pt_opcs_cbr		= 2
};

/** The psb magic payload.
 *
 * The payload is a repeating 2-byte pattern.
 */
enum pt_psb_pattern {
	/* The high and low bytes in the pattern. */
	pt_psb_hi		= pt_opc_psb,
	pt_psb_lo		= pt_ext_psb,

	/* Various combinations of the above parts. */
	pt_psb_lohi		= pt_psb_lo | pt_psb_hi << 8,
	pt_psb_hilo		= pt_psb_hi | pt_psb_lo << 8,

	/* The repeat count of the payload, not including opc and ext. */
	pt_psb_repeat_count	= 7,

	/* The size of the repeated pattern in bytes. */
	pt_psb_repeat_size	= 2
};

/** An execution mode. */
enum pt_exec_mode {
	ptem_unknown,
	ptem_16bit,
	ptem_32bit,
	ptem_64bit
};

/** The payload details. */
enum pt_payload {
	/* The shift counts for post-processing the PIP payload. */
	pt_pl_pip_shr		= 1,
	pt_pl_pip_shl		= 5,

	/* The size of a PIP payload in bytes. */
	pt_pl_pip_size		= 6,

	/* The size of a 8bit TNT packet's payload in bits. */
	pt_pl_tnt_8_bits	= 8 - pt_opm_tnt_8_shr,

	/* The size of a 64bit TNT packet's payload in bytes. */
	pt_pl_tnt_64_size	= 6,

	/* The size of a 64bit TNT packet's payload in bits. */
	pt_pl_tnt_64_bits	= 48,

	/* The size of a TSC packet's payload in bytes. */
	pt_pl_tsc_size		= 7,

	/* The size of a CBR packet's payload in bytes. */
	pt_pl_cbr_size		= 2,

	/* The size of a PSB packet's payload in bytes. */
	pt_pl_psb_size		= pt_psb_repeat_count * pt_psb_repeat_size,

	/* The size of a MODE packet's payload in bytes. */
	pt_pl_mode_size		= 1,

	/* The size of an IP packet's payload with update-16 compression. */
	pt_pl_ip_upd16_size	= 2,

	/* The size of an IP packet's payload with update-32 compression. */
	pt_pl_ip_upd32_size	= 4,

	/* The size of an IP packet's payload with sext-48 compression. */
	pt_pl_ip_sext48_size	= 6
};

/** Mode packet masks. */
enum pt_mode_mask {
	pt_mom_leaf		= 0xe0,
	pt_mom_leaf_shr		= 5,
	pt_mom_bits		= 0x1f
};

/** Mode packet leaves. */
enum pt_mode_leaf {
	pt_mol_exec		= 0x00,
	pt_mol_tsx		= 0x20
};

/** Mode packet bits. */
enum pt_mode_bit {
	/* mode.exec */
	pt_mob_exec_csl		= 0x01,
	pt_mob_exec_csd		= 0x02,

	/* mode.tsx */
	pt_mob_tsx_intx		= 0x01,
	pt_mob_tsx_abrt		= 0x02
};

/** The IP compression. */
enum pt_ip_compression {
	/* The bits encode the payload size and the encoding scheme.
	 *
	 * No payload.  The IP has been suppressed.
	 */
	pt_ipc_suppressed	= 0x0,

	/* Payload: 16 bits.  Update last IP. */
	pt_ipc_update_16	= 0x01,

	/* Payload: 32 bits.  Update last IP. */
	pt_ipc_update_32	= 0x02,

	/* Payload: 48 bits.  Sign extend to full address. */
	pt_ipc_sext_48		= 0x03
};

/** The size of the various packets in bytes. */
enum pt_packet_size {
	ptps_pad		= pt_opcs_pad,
	ptps_tnt_8		= pt_opcs_tnt_8,
	ptps_mode		= pt_opcs_mode + pt_pl_mode_size,
	ptps_tsc		= pt_opcs_tsc + pt_pl_tsc_size,
	ptps_psb		= pt_opcs_psb + pt_pl_psb_size,
	ptps_psbend		= pt_opcs_psbend,
	ptps_ovf		= pt_opcs_ovf,
	ptps_pip		= pt_opcs_pip + pt_pl_pip_size,
	ptps_tnt_64		= pt_opcs_tnt_64 + pt_pl_tnt_64_size,
	ptps_cbr		= pt_opcs_cbr + pt_pl_cbr_size,
	ptps_tip_supp		= pt_opcs_tip,
	ptps_tip_upd16		= pt_opcs_tip + pt_pl_ip_upd16_size,
	ptps_tip_upd32		= pt_opcs_tip + pt_pl_ip_upd32_size,
	ptps_tip_sext48		= pt_opcs_tip + pt_pl_ip_sext48_size,
	ptps_tip_pge_supp	= pt_opcs_tip_pge,
	ptps_tip_pge_upd16	= pt_opcs_tip_pge + pt_pl_ip_upd16_size,
	ptps_tip_pge_upd32	= pt_opcs_tip_pge + pt_pl_ip_upd32_size,
	ptps_tip_pge_sext48	= pt_opcs_tip_pge + pt_pl_ip_sext48_size,
	ptps_tip_pgd_supp	= pt_opcs_tip_pgd,
	ptps_tip_pgd_upd16	= pt_opcs_tip_pgd + pt_pl_ip_upd16_size,
	ptps_tip_pgd_upd32	= pt_opcs_tip_pgd + pt_pl_ip_upd32_size,
	ptps_tip_pgd_sext48	= pt_opcs_tip_pgd + pt_pl_ip_sext48_size,
	ptps_fup_supp		= pt_opcs_fup,
	ptps_fup_upd16		= pt_opcs_fup + pt_pl_ip_upd16_size,
	ptps_fup_upd32		= pt_opcs_fup + pt_pl_ip_upd32_size,
	ptps_fup_sext48		= pt_opcs_fup + pt_pl_ip_sext48_size
};



/* Errors. */



/** Error codes. */
enum pt_error_code {
	/* No error. Everything is OK. */
	pte_ok,

	/* Internal decoder error. */
	pte_internal,

	/* Invalid argument. */
	pte_invalid,

	/* Decoder out of sync. */
	pte_nosync,

	/* Unknown opcode. */
	pte_bad_opc,

	/* Unknown payload. */
	pte_bad_packet,

	/* Unexpected packet context. */
	pte_bad_context,

	/* Decoder reached end of trace stream. */
	pte_eos,

	/* No packet matching the query to be found. */
	pte_bad_query,

	/* Decoder out of memory. */
	pte_nomem,

	/* Bad configuration. */
	pte_bad_config,

	/* There is no IP. */
	pte_noip,

	/* The IP has been suppressed. */
	pte_ip_suppressed,

	/* There is no memory mapped at the requested address. */
	pte_nomap,

	/* An instruction could not be decoded. */
	pte_bad_insn,

	/* No timing information is available. */
	pte_no_time,

	/* No core:bus ratio available. */
	pte_no_cbr,

	/* Bad traced image. */
	pte_bad_image,

	/* A locking error. */
	pte_bad_lock,

	/* The requested feature is not supported. */
	pte_not_supported
};


/** Decode a function return value into an pt_error_code. */
static inline enum pt_error_code pt_errcode(int status)
{
	return (status >= 0) ? pte_ok : (enum pt_error_code) -status;
}

/** Return a human readable error string. */
extern pt_export const char *pt_errstr(enum pt_error_code);



/* Configuration. */



/** A cpu vendor. */
enum pt_cpu_vendor {
	pcv_unknown,
	pcv_intel
};

/** A cpu identifier. */
struct pt_cpu {
	/** The cpu vendor. */
	enum pt_cpu_vendor vendor;

	/** The cpu family. */
	uint16_t family;

	/** The cpu model. */
	uint8_t model;

	/** The stepping. */
	uint8_t stepping;
};

/** A collection of Intel PT errata. */
struct pt_errata {
	/** BDM70: Intel(R) Processor Trace PSB+ Packets May Contain
	 *         Unexpected Packets.
	 *
	 * Some Intel Processor Trace packets should be issued only between
	 * TIP.PGE and TIP.PGD packets.  Due to this erratum, when a TIP.PGE
	 * packet is generated it may be preceded by a PSB+ that incorrectly
	 * includes FUP and MODE.Exec packets.
	 */
	uint32_t bdm70:1;

	/** BDM64: An Incorrect LBR or Intel(R) Processor Trace Packet May Be
	 *         Recorded Following a Transactional Abort.
	 *
	 * Use of Intel(R) Transactional Synchronization Extensions (Intel(R)
	 * TSX) may result in a transactional abort.  If an abort occurs
	 * immediately following a branch instruction, an incorrect branch
	 * target may be logged in an LBR (Last Branch Record) or in an Intel(R)
	 * Processor Trace (Intel(R) PT) packet before the LBR or Intel PT
	 * packet produced by the abort.
	 */
	uint32_t bdm64:1;
};

/** An unknown packet. */
struct pt_packet_unknown;

/** An Intel PT decoder configuration.
 */
struct pt_config {
	/** The size of the config structure in bytes. */
	size_t size;

	/** The trace buffer begin address. */
	uint8_t *begin;

	/** The trace buffer end address. */
	uint8_t *end;

	/** An optional callback for handling unknown packets.
	 *
	 * If \@callback is not NULL, it is called for any unknown opcode.
	 */
	struct {
		/** The callback function.
		 *
		 * It shall decode the packet at \@pos into \@unknown.
		 * It shall return the number of bytes read upon success.
		 * It shall return a negative pt_error_code otherwise.
		 * The below context is passed as \@context.
		 */
		int (*callback)(struct pt_packet_unknown *unknown,
				const struct pt_config *config,
				const uint8_t *pos, void *context);

		/** The user-defined context for this configuration. */
		void *context;
	} decode;

	/** The cpu on which Intel PT has been recorded. */
	struct pt_cpu cpu;

	/** The errata to apply when encoding or decoding Intel PT. */
	struct pt_errata errata;
};


/** Zero-initialize an Intel PT configuration. */
static inline void pt_config_init(struct pt_config *config)
{
	memset(config, 0, sizeof(*config));

	config->size = sizeof(*config);
}

/** Determine errata for a given cpu.
 *
 * Updates \@errata based on \@cpu.
 *
 * Returns 0 on success, a negative error code otherwise.
 * Returns -pte_invalid if \@errata or \@cpu is NULL.
 */
extern pt_export int pt_cpu_errata(struct pt_errata *errata,
				   const struct pt_cpu *cpu);



/* Packet encoder / decoder. */



/** Intel PT packet types. */
enum pt_packet_type {
	/* 1-byte header packets. */
	ppt_pad			= pt_opc_pad,
	ppt_tip			= pt_opc_tip,
	ppt_tnt_8		= pt_opc_tnt_8 | 0xFE,
	ppt_tip_pge		= pt_opc_tip_pge,
	ppt_tip_pgd		= pt_opc_tip_pgd,
	ppt_fup			= pt_opc_fup,
	ppt_mode		= pt_opc_mode,
	ppt_tsc			= pt_opc_tsc,

	/* 2-byte header packets. */
	ppt_psb			= pt_opc_ext << 8 | pt_ext_psb,
	ppt_tnt_64		= pt_opc_ext << 8 | pt_ext_tnt_64,
	ppt_pip			= pt_opc_ext << 8 | pt_ext_pip,
	ppt_ovf			= pt_opc_ext << 8 | pt_ext_ovf,
	ppt_psbend		= pt_opc_ext << 8 | pt_ext_psbend,
	ppt_cbr			= pt_opc_ext << 8 | pt_ext_cbr,

	/* A packet decodable by the optional decoder callback. */
	ppt_unknown		= 0x7ffffffe,

	/* An invalid packet. */
	ppt_invalid		= 0x7fffffff
};

/** A TNT-8 or TNT-64 packet. */
struct pt_packet_tnt {
	/** TNT payload bit size. */
	uint8_t bit_size;

	/** TNT payload excluding stop bit. */
	uint64_t payload;
};

/** A packet with IP payload. */
struct pt_packet_ip {
	/** IP compression. */
	enum pt_ip_compression ipc;

	/** Zero-extended payload ip. */
	uint64_t ip;
};

/** A mode.exec packet. */
struct pt_packet_mode_exec {
	/** The mode.exec csl bit. */
	uint32_t csl:1;

	/** The mode.exec csd bit. */
	uint32_t csd:1;
};

static inline enum pt_exec_mode
pt_get_exec_mode(const struct pt_packet_mode_exec *packet)
{
	if (packet->csl)
		return packet->csd ? ptem_unknown : ptem_64bit;
	else
		return packet->csd ? ptem_32bit : ptem_16bit;
}

static inline struct pt_packet_mode_exec
pt_set_exec_mode(enum pt_exec_mode mode)
{
	struct pt_packet_mode_exec packet;

	switch (mode) {
	default:
		packet.csl = 1;
		packet.csd = 1;
		break;

	case ptem_64bit:
		packet.csl = 1;
		packet.csd = 0;
		break;

	case ptem_32bit:
		packet.csl = 0;
		packet.csd = 1;
		break;

	case ptem_16bit:
		packet.csl = 0;
		packet.csd = 0;
		break;
	}

	return packet;
}

/** A mode.tsx packet. */
struct pt_packet_mode_tsx {
	/** The mode.tsx intx bit. */
	uint32_t intx:1;

	/** The mode.tsx abrt bit. */
	uint32_t abrt:1;
};

/** A mode packet. */
struct pt_packet_mode {
	/** Mode leaf. */
	enum pt_mode_leaf leaf;

	/** Mode bits. */
	union {
		/** Packet: mode.exec. */
		struct pt_packet_mode_exec exec;

		/** Packet: mode.tsx. */
		struct pt_packet_mode_tsx tsx;
	} bits;
};

/** A PIP packet. */
struct pt_packet_pip {
	/** The CR3 value. */
	uint64_t cr3;
};

/** A TSC packet. */
struct pt_packet_tsc {
	/** The TSC value. */
	uint64_t tsc;
};

/** A CBR packet. */
struct pt_packet_cbr {
	/** The core/bus cycle ratio. */
	uint8_t ratio;
};

/** An unknown packet decodable by the optional decoder callback. */
struct pt_packet_unknown {
	/** Pointer to the raw packet bytes. */
	const uint8_t *packet;

	/** Optional pointer to a user-defined structure. */
	void *priv;
};

/** An Intel PT packet. */
struct pt_packet {
	/** Type of the packet, used to indicate which sub-struct to use. */
	enum pt_packet_type type;

	/** Size of the packet, including opcode and payload. */
	uint8_t size;

	/** Packet specific data. */
	union {
		/** Packets: pad, ovf, psb, psbend - no payload. */

		/** Packet: tnt-8, tnt-64. */
		struct pt_packet_tnt tnt;

		/** Packet: tip, fup, tip.pge, tip.pgd. */
		struct pt_packet_ip ip;

		/** Packet: mode. */
		struct pt_packet_mode mode;

		/** Packet: pip. */
		struct pt_packet_pip pip;

		/** Packet: tsc. */
		struct pt_packet_tsc tsc;

		/** Packet: cbr. */
		struct pt_packet_cbr cbr;

		/** Packet: unknown. */
		struct pt_packet_unknown unknown;
	} payload;
};



/* Packet encoder. */



/** Allocate an Intel PT packet encoder.
 *
 * The encoder will work on the buffer defined in \@config, it shall contain
 * raw trace data and remain valid for the lifetime of the encoder.
 *
 * The encoder starts at the beginning of the trace buffer.
 */
extern pt_export struct pt_encoder *
pt_alloc_encoder(const struct pt_config *config);

/** Free an Intel PT packet encoder.
 *
 * The \@encoder must not be used after a successful return.
 */
extern pt_export void pt_free_encoder(struct pt_encoder *encoder);

/** Hard set synchronization point of an Intel PT packet encoder.
 *
 * Synchronize \@encoder to \@offset within the trace buffer.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_eos if the given offset is behind the end of the trace buffer.
 * Returns -pte_invalid if \@encoder is NULL.
 */
extern pt_export int pt_enc_sync_set(struct pt_encoder *encoder,
				     uint64_t offset);

/** Get the current packet encoder position.
 *
 * Fills the current \@encoder position into \@offset.
 *
 * This is useful for reporting errors.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@encoder or \@offset is NULL.
 * Returns -pte_nosync if \@encoder is out of sync.
 */
extern pt_export int pt_enc_get_offset(struct pt_encoder *encoder,
				       uint64_t *offset);

/* Return a pointer to \@encoder's configuration.
 *
 * Returns a non-null pointer on success, NULL if \@encoder is NULL.
 */
extern pt_export const struct pt_config *
pt_enc_get_config(const struct pt_encoder *encoder);

/** Encode an Intel PT packet.
 *
 * Writes \@packet at \@encoder's current position in the Intel PT buffer and
 * advances the \@encoder beyond the written packet.
 *
 * The \@packet.size field is ignored.
 *
 * In case of errors, the \@encoder is not advanced and nothing is written
 * into the Intel PT buffer.
 *
 * Returns the number of bytes written on success, a negative error code
 * otherwise.
 *
 * Returns -pte_bad_opc if \@packet.type is not known.
 * Returns -pte_bad_packet if \@packet's payload is invalid.
 * Returns -pte_eos if \@encoder reached the end of the Intel PT buffer.
 * Returns -pte_invalid if \@encoder or \@packet is NULL.
 * Returns -pte_nosync if \@encoder is out of sync.
 */
extern pt_export int pt_enc_next(struct pt_encoder *encoder,
				 const struct pt_packet *packet);



/* Packet decoder. */



/** Allocate an Intel PT packet decoder.
 *
 * The decoder will work on the buffer defined in \@config, it shall contain
 * raw trace data and remain valid for the lifetime of the decoder.
 *
 * The decoder needs to be synchronized before it can be used.
 */
extern pt_export struct pt_packet_decoder *
pt_pkt_alloc_decoder(const struct pt_config *config);

/** Free an Intel PT packet decoder.
 *
 * The \@decoder must not be used after a successful return.
 */
extern pt_export void pt_pkt_free_decoder(struct pt_packet_decoder *decoder);

/** Synchronize an Intel PT packet decoder.
 *
 * Search for the next synchronization point in forward or backward direction.
 *
 * If \@decoder has not been synchronized, yet, the search is started at the
 * beginning of the trace buffer in case of forward synchronization and at the
 * end of the trace buffer in case of backward synchronization.
 *
 * Returns zero or a positive value on success, a negative error code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_eos if no further synchronization point is found.
 * Returns -pte_invalid if \@decoder is NULL.
 */
extern pt_export int pt_pkt_sync_forward(struct pt_packet_decoder *decoder);
extern pt_export int pt_pkt_sync_backward(struct pt_packet_decoder *decoder);

/** Hard set synchronization point of an Intel PT decoder.
 *
 * Synchronize \@decoder to \@offset within the trace buffer.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_eos if the given offset is behind the end of the trace buffer.
 * Returns -pte_invalid if \@decoder is NULL.
 */
extern pt_export int pt_pkt_sync_set(struct pt_packet_decoder *decoder,
				     uint64_t offset);

/** Get the current decoder position.
 *
 * Fills the current \@decoder position into \@offset.
 *
 * This is useful for reporting errors.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@offset is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_pkt_get_offset(struct pt_packet_decoder *decoder,
				       uint64_t *offset);

/** Get the position of the last synchronization point.
 *
 * Fills the last synchronization position into \@offset.
 *
 * This is useful when splitting a trace stream for parallel decoding.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@offset is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_pkt_get_sync_offset(struct pt_packet_decoder *decoder,
					    uint64_t *offset);

/* Return a pointer to \@decoder's configuration.
 *
 * Returns a non-null pointer on success, NULL if \@decoder is NULL.
 */
extern pt_export const struct pt_config *
pt_pkt_get_config(const struct pt_packet_decoder *decoder);

/** Decode the next packet and advance the decoder.
 *
 * Decodes the packet at \@decoder's current position into \@packet and
 * adjusts the \@decoder's position by the number of bytes the packet had
 * consumed.
 *
 * Returns the number of bytes consumed on success, a negative error code
 * otherwise.
 *
 * Returns -pte_bad_opc if the packet is unknown.
 * Returns -pte_eos if \@decoder reached the end of the Intel PT buffer.
 * Returns -pte_invalid if \@decoder or \@packet is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_pkt_next(struct pt_packet_decoder *decoder,
				 struct pt_packet *packet);



/* Query decoder. */



/** Decoder status flags. */
enum pt_status_flag {
	/* There is an event pending. */
	pts_event_pending	= 1 << 0,

	/* The destination address has been suppressed due to CPL filtering. */
	pts_ip_suppressed	= 1 << 1,

	/* There is no more trace data available. */
	pts_eos			= 1 << 2
};

/** Event types. */
enum pt_event_type {
	/* Tracing has been enabled/disabled. */
	ptev_enabled,
	ptev_disabled,

	/* Tracing has been disabled asynchronously. */
	ptev_async_disabled,

	/* An asynchronous branch, e.g. interrupt. */
	ptev_async_branch,

	/* A synchronous paging event. */
	ptev_paging,

	/* An asynchronous paging event. */
	ptev_async_paging,

	/* Trace overflow. */
	ptev_overflow,

	/* An execution mode change. */
	ptev_exec_mode,

	/* A transactional execution state change. */
	ptev_tsx
};

/** An event. */
struct pt_event {
	/** The type of the event. */
	enum pt_event_type type;

	/** A flag indicating that the event IP had been suppressed. */
	uint32_t ip_suppressed:1;

	/** A flag indicating that the event is for status update. */
	uint32_t status_update:1;

	/** A flag indicating that the event has timing information. */
	uint32_t has_tsc:1;

	/** The time stamp count of the event.
	 *
	 * This field is only valid if \@has_tsc is set.
	 */
	uint64_t tsc;

	/* Reserved space for future extensions. */
	uint64_t reserved[3];

	/** Event specific data. */
	union {
		/** Event: enabled. */
		struct {
			/* The address at which tracing resumes. */
			uint64_t ip;
		} enabled;

		/** Event: disabled. */
		struct {
			/** The destination of the first branch inside a
			 * filtered area.
			 *
			 * This field is not valid, if pts_ip_suppressed is
			 * returned from the query function.
			 */
			uint64_t ip;

			/* The exact source ip needs to be determined using
			 * disassembly and the filter configuration.
			 */
		} disabled;

		/** Event: async disabled. */
		struct {
			/** The source address of the asynchronous branch that
			 * disabled tracing.
			 */
			uint64_t at;

			/** The destination of the first branch inside a
			 * filtered area.
			 *
			 * This field is not valid, if pts_ip_suppressed is
			 * returned from the query function.
			 */
			uint64_t ip;
		} async_disabled;

		/** Event: async branch. */
		struct {
			/** The branch source address. */
			uint64_t from;

			/** The branch destination address.
			 *
			 * This field is not valid, if pts_ip_suppressed is
			 * returned from the query function.
			 */
			uint64_t to;
		} async_branch;

		/** Event: paging. */
		struct {
			/** The updated CR3 value.
			 *
			 * The lower 5 bit have been zeroed out.
			 * The upper bits have been zeroed out depending on the
			 * maximum possible address.
			 */
			uint64_t cr3;

			/* The address at which the event is effective is
			 * obvious from the disassembly.
			 */
		} paging;

		/** Event: async paging. */
		struct {
			/** The updated CR3 value.
			 *
			 * The lower 5 bit have been zeroed out.
			 * The upper bits have been zeroed out depending on the
			 * maximum possible address.
			 */
			uint64_t cr3;

			/** The address at which the event is effective. */
			uint64_t ip;
		} async_paging;

		/** Event: overflow. */
		struct {
			/** The address at which tracing resumes after overflow.
			 */
			uint64_t ip;
		} overflow;

		/** Event: exec mode. */
		struct {
			/** The execution mode. */
			enum pt_exec_mode mode;

			/** The address at which the event is effective. */
			uint64_t ip;
		} exec_mode;

		/** Event: tsx. */
		struct {
			/** The address at which the event is effective.
			 *
			 * This field is not valid, if pts_ip_suppressed is
			 * returned from the query function.
			 */
			uint64_t ip;

			/** A flag indicating speculative execution mode. */
			uint32_t speculative:1;

			/** A flag indicating speculative execution aborts. */
			uint32_t aborted:1;
		} tsx;
	} variant;
};


/** Allocate an Intel PT query decoder.
 *
 * The decoder will work on the buffer defined in \@config, it shall contain
 * raw trace data and remain valid for the lifetime of the decoder.
 *
 * The decoder needs to be synchronized before it can be used.
 */
extern pt_export struct pt_query_decoder *
pt_qry_alloc_decoder(const struct pt_config *config);

/** Free an Intel PT query decoder.
 *
 * The \@decoder must not be used after a successful return.
 */
extern pt_export void pt_qry_free_decoder(struct pt_query_decoder *decoder);

/** Synchronize an Intel PT query decoder.
 *
 * Search for the next synchronization point in forward or backward direction.
 *
 * If \@decoder has not been synchronized, yet, the search is started at the
 * beginning of the trace buffer in case of forward synchronization and at the
 * end of the trace buffer in case of backward synchronization.
 *
 * If \@ip is not NULL, set it to last ip.
 *
 * Returns a non-negative pt_status_flag bit-vector on success, a negative error
 * code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_eos if no further synchronization point is found.
 * Returns -pte_invalid if \@decoder is NULL.
 */
extern pt_export int pt_qry_sync_forward(struct pt_query_decoder *decoder,
					 uint64_t *ip);
extern pt_export int pt_qry_sync_backward(struct pt_query_decoder *decoder,
					 uint64_t *ip);

/** Manually synchronize an Intel PT query decoder.
 *
 * Synchronize \@decoder on the syncpoint at \@offset.  There must be a PSB
 * packet at \@offset.
 *
 * If \@ip is not NULL, set it to last ip.
 *
 * Returns a non-negative pt_status_flag bit-vector on success, a negative error
 * code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_eos if \@decoder reaches the end of its trace buffer.
 * Returns -pte_invalid if \@decoder is NULL.
 * Returns -pte_invalid if \@offset lies outside of \@decoder's trace buffer.
 * Returns -pte_nosync if there is no syncpoint at \@offset.
 */
extern pt_export int pt_qry_sync_set(struct pt_query_decoder *decoder,
				     uint64_t *ip, uint64_t offset);

/** Get the current decoder position.
 *
 * Fills the current \@decoder position into \@offset.
 *
 * This is useful for reporting errors.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@offset is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_qry_get_offset(struct pt_query_decoder *decoder,
				       uint64_t *offset);

/** Get the position of the last synchronization point.
 *
 * Fills the last synchronization position into \@offset.
 *
 * This is useful for splitting a trace stream for parallel decoding.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@offset is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_qry_get_sync_offset(struct pt_query_decoder *decoder,
					    uint64_t *offset);

/* Return a pointer to \@decoder's configuration.
 *
 * Returns a non-null pointer on success, NULL if \@decoder is NULL.
 */
extern pt_export const struct pt_config *
pt_qry_get_config(const struct pt_query_decoder *decoder);

/** Query whether the next unconditional branch has been taken.
 *
 * On success, provides 1 (taken) or 0 (not taken) in \@taken for the next
 * conditional branch and updates \@decoder.
 *
 * Returns a non-negative pt_status_flag bit-vector on success, a negative error
 * code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_bad_query if no conditional branch is found.
 * Returns -pte_eos if decoding reached the end of the Intel PT buffer.
 * Returns -pte_invalid if \@decoder or \@taken is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_qry_cond_branch(struct pt_query_decoder *decoder,
					int *taken);

/** Get the next indirect branch destination.
 *
 * On success, provides the linear destination address of the next indirect
 * branch in \@ip and updates \@decoder.
 *
 * Returns a non-negative pt_status_flag bit-vector on success, a negative error
 * code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_bad_query if no indirect branch is found.
 * Returns -pte_eos if decoding reached the end of the Intel PT buffer.
 * Returns -pte_invalid if \@decoder or \@ip is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_qry_indirect_branch(struct pt_query_decoder *decoder,
					    uint64_t *ip);

/** Query the next pending event.
 *
 * On success, provides the next event \@event and updates \@decoder.
 *
 * The \@size argument must be set to sizeof(struct pt_event).
 *
 * Returns a non-negative pt_status_flag bit-vector on success, a negative error
 * code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_bad_query if no event is found.
 * Returns -pte_eos if decoding reached the end of the Intel PT buffer.
 * Returns -pte_invalid if \@decoder or \@event is NULL.
 * Returns -pte_invalid if \@size is too small.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_qry_event(struct pt_query_decoder *decoder,
				  struct pt_event *event, size_t size);

/** Query the current time.
 *
 * On success, provides the time at \@decoder's current position in \@time.
 * Since \@decoder is reading ahead until the next indirect branch or event,
 * the value matches the time for that branch or event.
 *
 * The time is similar to what an rdtsc instruction would return. Beware that
 * the time is not fully accurate. It should be good enough for a course
 * correlation with other time stamped data such as side-band information.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@time is NULL.
 */
extern pt_export int pt_qry_time(struct pt_query_decoder *decoder,
				 uint64_t *time);

/** Return the current core bus ratio.
 *
 * On success, provides the core:bus ratio at \@decoder's current position
 * in \@cbr.
 * Since \@decoder is reading ahead until the next indirect branch or event,
 * the value matches the core:bus ratio for that branch or event.
 *
 * The ratio is defined as core cycles per bus clock cycle.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@cbr is NULL.
 */
extern pt_export int pt_qry_core_bus_ratio(struct pt_query_decoder *decoder,
					   uint32_t *cbr);



/* Traced image. */



/** An Intel PT address space identifier.
 *
 * This identifies a particular address space when adding file sections or
 * when reading memory.
 */
struct pt_asid {
	/** The size of this object - set to sizeof(struct pt_asid). */
	size_t size;

	/** The CR3 value. */
	uint64_t cr3;
};

/** An unknown CR3 value to be used for pt_asid objects. */
static const uint64_t pt_asid_no_cr3 = 0xffffffffffffffffull;

/** Initialize an address space identifier. */
static inline void pt_asid_init(struct pt_asid *asid)
{
	asid->size = sizeof(*asid);
	asid->cr3 = pt_asid_no_cr3;
}


/** The traced memory image. */
struct pt_image;


/** Allocate a traced memory image.
 *
 * An optional \@name may be given to the image.  The name string is copied.
 *
 * Returns a new traced memory image on success, NULL otherwise.
 */
extern pt_export struct pt_image *pt_image_alloc(const char *name);

/** Free a traced memory image.
 *
 * The \@image must have been allocated with pt_image_alloc().
 * The \@image must not be used after a successful return.
 */
extern pt_export void pt_image_free(struct pt_image *image);

/** Get the image name.
 *
 * Returns a pointer to \@image's name or NULL if there is no name.
 */
extern pt_export const char *pt_image_name(const struct pt_image *image);

/** Add a new file section to the traced memory image.
 *
 * Adds \@size bytes starting at \@offset in \@filename. The section is
 * loaded at the virtual address \@vaddr in the address space \@asid.
 *
 * The \@asid may be NULL or (partially) invalid.  In that case only the valid
 * fields are considered when comparing with other address-spaces.  Use this
 * when tracing a single process or when adding sections to all processes.
 *
 * The section is silently truncated to match the size of \@filename.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_bad_image if sections would overlap.
 * Returns -pte_invalid if \@image or \@filename is NULL.
 * Returns -pte_invalid if \@offset is too big.
 */
extern pt_export int pt_image_add_file(struct pt_image *image,
				       const char *filename, uint64_t offset,
				       uint64_t size,
				       const struct pt_asid *asid,
				       uint64_t vaddr);

/** Copy an image.
 *
 * Adds all sections from \@src to \@image.  Sections that would overlap with
 * existing sections will be ignored.
 *
 * Returns the number of ignored images on success, a negative error code
 * otherwise.
 *
 * Returns -pte_invalid if \@image or \@src is NULL.
 */
extern pt_export int pt_image_copy(struct pt_image *image,
				   const struct pt_image *src);

/** Remove all sections loaded from a file.
 *
 * Removes all sections loaded from \@filename from the address space \@asid.
 * Specify the same \@asid that was used for adding sections from \@filename.
 *
 * Returns the number of removed sections on success, a negative error code
 * otherwise.
 *
 * Returns -pte_invalid if \@image or \@filename is NULL.
 */
extern pt_export int pt_image_remove_by_filename(struct pt_image *image,
						 const char *filename,
						 const struct pt_asid *asid);

/** Remove all sections loaded into an address space.
 *
 * Removes all sections loaded into \@asid.  Specify the same \@asid that was
 * used for adding sections.
 *
 * Returns the number of removed sections on success, a negative error code
 * otherwise.
 *
 * Returns -pte_invalid if \@image is NULL.
 */
extern pt_export int pt_image_remove_by_asid(struct pt_image *image,
					     const struct pt_asid *asid);

/** A read memory callback function.
 *
 * It shall read \@size bytes of memory from address space \@asid starting
 * at \@ip into \@buffer.
 *
 * It shall return the number of bytes read on success.
 * It shall return a negative pt_error_code otherwise.
 */
typedef int (read_memory_callback_t)(uint8_t *buffer, size_t size,
				     const struct pt_asid *asid,
				     uint64_t ip, void *context);

/** Set the memory callback for the traced memory image.
 *
 * Sets \@callback for reading memory.  The callback is used for addresses
 * that are not found in file sections.  The \@context argument is passed
 * to \@callback on each use.
 *
 * There can only be one callback at any time.  A subsequent call will replace
 * the previous callback.  If \@callback is NULL, the callback is removed.
 *
 * Returns -pte_invalid if \@image is NULL.
 */
extern pt_export int pt_image_set_callback(struct pt_image *image,
					   read_memory_callback_t *callback,
					   void *context);

/** A new CR3 callback function
 *
 * Called when the decoder sees a new unknown CR3 value. Useful to load
 * binaries lazily only when they are encountered in the trace.
 * Can add new files to the image.
 *
 * \@image Image pointer
 * \@cr3   New cr3 value
 * \@context User defined context
 * \@ip    Current IP
 *
 * It shall return zero on success.
 * It shall return a negative pt_error_code otherwise.
 */

typedef int (new_cr3_callback_t)(struct pt_image *image, uint64_t cr3, void *context, uint64_t ip);

/** Set the new_cr3 callback for the traced memory image.
 *
 * Sets \@callback for handling unknown CR3 values.
 * The \@context argument is passed to \@callback on each use.
 *
 * There can only be one callback at any time.  A subsequent call will replace
 * the previous callback.  If \@callback is NULL, the callback is removed.
 *
 * Returns -pte_invalid if \@image is NULL.
 */

extern pt_export int pt_image_set_new_cr3_callback(struct pt_image *image,
					   new_cr3_callback_t *callback,
					   void *context);

/* Instruction flow decoder. */



/** The instruction class.
 *
 * We provide only a very coarse classification suitable for reconstructing
 * the execution flow.
 */
enum pt_insn_class {
	/* The instruction could not be classified. */
	ptic_error,

	/* The instruction is something not listed below. */
	ptic_other,

	/* The instruction is a near (function) call. */
	ptic_call,

	/* The instruction is a near (function) return. */
	ptic_return,

	/* The instruction is a near unconditional jump. */
	ptic_jump,

	/* The instruction is a near conditional jump. */
	ptic_cond_jump,

	/* The instruction is a call-like far transfer.
	 * E.g. SYSCALL, SYSENTER, or FAR CALL.
	 */
	ptic_far_call,

	/* The instruction is a return-like far transfer.
	 * E.g. SYSRET, SYSEXIT, IRET, or FAR RET.
	 */
	ptic_far_return,

	/* The instruction is a jump-like far transfer.
	 * E.g. FAR JMP.
	 */
	ptic_far_jump
};

/** The maximal size of an instruction. */
enum {
	pt_max_insn_size	= 15
};

/** A single traced instruction. */
struct pt_insn {
	/** The virtual address in its process. */
	uint64_t ip;

	/** A coarse classification. */
	enum pt_insn_class iclass;

	/** The execution mode. */
	enum pt_exec_mode mode;

	/** The raw bytes. */
	uint8_t raw[pt_max_insn_size];

	/** The size in bytes. */
	uint8_t size;

	/** A collection of flags giving additional information:
	 *
	 * - the instruction was executed speculatively.
	 */
	uint32_t speculative:1;

	/** - speculative execution was aborted after this instruction. */
	uint32_t aborted:1;

	/** - speculative execution was committed after this instruction. */
	uint32_t committed:1;

	/** - tracing was disabled after this instruction. */
	uint32_t disabled:1;

	/** - tracing was enabled at this instruction. */
	uint32_t enabled:1;

	/** - tracing was resumed at this instruction.
	 *
	 *    In addition to tracing being enabled, it continues from the IP
	 *    at which tracing had been disabled before.
	 */
	uint32_t resumed:1;

	/** - normal execution flow was interrupted after this instruction. */
	uint32_t interrupted:1;

	/** - tracing resumed at this instruction after an overflow. */
	uint32_t resynced:1;
};


/** Allocate an Intel PT instruction flow decoder.
 *
 * The decoder will work on the buffer defined in \@config, it shall contain
 * raw trace data and remain valid for the lifetime of the decoder.
 *
 * The decoder needs to be synchronized before it can be used.
 */
extern pt_export struct pt_insn_decoder *
pt_insn_alloc_decoder(const struct pt_config *config);

/** Free an Intel PT instruction flow decoder.
 *
 * This will destroy the decoder's default image.
 *
 * The \@decoder must not be used after a successful return.
 */
extern pt_export void pt_insn_free_decoder(struct pt_insn_decoder *decoder);

/** Synchronize an Intel PT instruction flow decoder.
 *
 * Search for the next synchronization point in forward or backward direction.
 *
 * If \@decoder has not been synchronized, yet, the search is started at the
 * beginning of the trace buffer in case of forward synchronization and at the
 * end of the trace buffer in case of backward synchronization.
 *
 * Returns zero or a positive value on success, a negative error code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_eos if no further synchronization point is found.
 * Returns -pte_invalid if \@decoder is NULL.
 */
extern pt_export int pt_insn_sync_forward(struct pt_insn_decoder *decoder);
extern pt_export int pt_insn_sync_backward(struct pt_insn_decoder *decoder);

/** Manually synchronize an Intel PT instruction flow decoder.
 *
 * Synchronize \@decoder on the syncpoint at \@offset.  There must be a PSB
 * packet at \@offset.
 *
 * Returns zero or a positive value on success, a negative error code otherwise.
 *
 * Returns -pte_bad_opc if an unknown packet is encountered.
 * Returns -pte_bad_packet if an unknown packet payload is encountered.
 * Returns -pte_eos if \@decoder reaches the end of its trace buffer.
 * Returns -pte_invalid if \@decoder is NULL.
 * Returns -pte_invalid if \@offset lies outside of \@decoder's trace buffer.
 * Returns -pte_nosync if there is no syncpoint at \@offset.
 */
extern pt_export int pt_insn_sync_set(struct pt_insn_decoder *decoder,
				      uint64_t offset);

/** Get the current decoder position.
 *
 * Fills the current \@decoder position into \@offset.
 *
 * This is useful for reporting errors.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@offset is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_insn_get_offset(struct pt_insn_decoder *decoder,
					uint64_t *offset);

/** Get the current CR3
 *
 * Fills the current CR3 of \@decoder into \@cr3.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@offset is NULL.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_insn_get_cr3(struct pt_insn_decoder *decoder,
				     uint64_t *cr3);


/** Get the traced image.
 *
 * The returned image may be modified as long as no decoder that uses this
 * image is running.
 *
 * Returns a pointer to the traced image the decoder uses for reading memory.
 * Returns NULL if \@decoder is NULL.
 */
extern pt_export struct pt_image *
pt_insn_get_image(struct pt_insn_decoder *decoder);

/** Set the traced image.
 *
 * Sets the image that \@decoder uses for reading memory to \@image.  If \@image
 * is NULL, sets the image to \@decoder's default image.
 *
 * Only one image can be active at any time.
 *
 * Returns zero on success, a negative error code otherwise.
 * Return -pte_invalid if \@decoder is NULL.
 */
extern pt_export int pt_insn_set_image(struct pt_insn_decoder *decoder,
				       struct pt_image *image);

/* Return a pointer to \@decoder's configuration.
 *
 * Returns a non-null pointer on success, NULL if \@decoder is NULL.
 */
extern pt_export const struct pt_config *
pt_insn_get_config(const struct pt_insn_decoder *decoder);

/** Return the current time.
 *
 * On success, provides the time at \@decoder's current position in \@time.
 * Since \@decoder is reading ahead until the next indirect branch or event,
 * the value matches the time for that branch or event.
 *
 * The time is similar to what an rdtsc instruction would return. Beware that
 * the time is not fully accurate. It should be good enough for a course
 * correlation with other time stamped data such as side-band information.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@time is NULL.
 */
extern pt_export int pt_insn_time(struct pt_insn_decoder *decoder,
				  uint64_t *time);

/** Return the current core bus ratio.
 *
 * On success, provides the core:bus ratio at \@decoder's current position
 * in \@cbr.
 * Since \@decoder is reading ahead until the next indirect branch or event,
 * the value matches the core:bus ratio for that branch or event.
 *
 * The ratio is defined as core cycles per bus clock cycle.
 *
 * Returns zero on success, a negative error code otherwise.
 *
 * Returns -pte_invalid if \@decoder or \@cbr is NULL.
 */
extern pt_export int pt_insn_core_bus_ratio(struct pt_insn_decoder *decoder,
					    uint32_t *cbr);

/** Determine the next instruction.
 *
 * On success, provides the next instruction in execution order in \@insn.
 *
 * The \@size argument must be set to sizeof(struct pt_insn).
 *
 * Returns a non-negative pt_status_flag bit-vector on success, a negative error
 * code otherwise.
 *
 * Returns pts_eos to indicate the end of the trace stream.  Subsequent calls
 * to pt_insn_next() will continue to return pts_eos until trace is required
 * to determine the next instruction.
 *
 * Returns -pte_bad_context if the decoder encountered an unexpected packet.
 * Returns -pte_bad_opc if the decoder encountered unknown packets.
 * Returns -pte_bad_packet if the decoder encountered unknown packet payloads.
 * Returns -pte_bad_query if the decoder got out of sync.
 * Returns -pte_eos if decoding reached the end of the Intel PT buffer.
 * Returns -pte_invalid if \@decoder or \@insn is NULL.
 * Returns -pte_nomap if the memory at the instruction address can't be read.
 * Returns -pte_nosync if \@decoder is out of sync.
 */
extern pt_export int pt_insn_next(struct pt_insn_decoder *decoder,
				  struct pt_insn *insn, size_t size);

#endif /* __INTEL_PT_H__ */
