/* SPDX-License-Identifier: GPL-2.0-only */

    .section .rodata.relocator, "a", %progbits
    .balign 4
    .global relocator_blob_start
relocator_blob_start:
    .incbin "relocator.bin"
    .global relocator_blob_end
relocator_blob_end:

    .section .rodata.compressed, "a", %progbits
    .balign 4
    .global compressed_blob_start
compressed_blob_start:
    .incbin "u-boot.deflate"
    .global compressed_blob_end
compressed_blob_end:

