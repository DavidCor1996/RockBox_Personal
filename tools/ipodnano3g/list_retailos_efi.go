package main

import (
	"bytes"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/freemyipod/wInd3x/pkg/efi"
	"github.com/freemyipod/wInd3x/pkg/image"
)

func uiName(f *efi.FirmwareFile) string {
	for _, s := range f.Sections {
		if s.Header().Type == efi.SectionTypeUserInterface {
			raw := bytes.ReplaceAll(s.Raw(), []byte{0}, nil)
			return string(raw)
		}
	}
	return ""
}

func firstPE32Info(f *efi.FirmwareFile) (int, int) {
	for _, s := range f.Sections {
		if s.Header().Type == efi.SectionTypePE32 {
			return int(s.Header().Type), len(s.Raw())
		}
	}
	return -1, 0
}

func main() {
	if len(os.Args) != 2 {
		log.Fatalf("usage: %s <decrypted-img1>", os.Args[0])
	}

	data, err := os.ReadFile(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}

	img, err := image.Read(bytes.NewReader(data))
	if err != nil {
		log.Fatal(err)
	}

	vol, err := efi.ReadVolume(efi.NewNestedReader(img.Body))
	if err != nil {
		log.Fatal(err)
	}

	fmt.Printf("files=%d\n", len(vol.Files))
	for i, f := range vol.Files {
		name := uiName(f)
		_, pe32Len := firstPE32Info(f)
		if name == "" {
			name = "-"
		}
		name = strings.ReplaceAll(name, "\n", " ")
		fmt.Printf("%03d guid=%s type=%d name=%q pe32=%d read_off=0x%x\n",
			i, f.GUID.String(), f.FileType, name, pe32Len, f.ReadOffset)
	}
}
