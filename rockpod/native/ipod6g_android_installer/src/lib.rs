use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::fmt;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
#[cfg(unix)]
use std::os::unix::fs::MetadataExt;
use std::path::{Path, PathBuf};

pub const PROTOCOL_VERSION: u32 = 1;
pub const HELPER_VERSION: &str = env!("CARGO_PKG_VERSION");
pub const MIN_ANDROID_MIB: u64 = 512;
pub const DEFAULT_ANDROID_MIB: u64 = 1024;
pub const ALIGNMENT_BYTES: u64 = 1024 * 1024;
pub const FAT_MARGIN_BYTES: u64 = 128 * 1024 * 1024;

const MIB: u64 = 1024 * 1024;
const MBR_SIGNATURE_OFFSET: usize = 510;
const MBR_PARTITION_OFFSET: usize = 446;
const MBR_PARTITION_SIZE: usize = 16;
const FAT32_TYPES: [u8; 2] = [0x0b, 0x0c];
const TRANSACTION_MAGIC: &[u8] = b"ROCKPOD-ANDROID-TXN-V1";
const TRANSACTION_HEADER_BYTES: usize = 68;

#[derive(Debug)]
pub enum InstallerError {
    InvalidArgument(String),
    UnsafeTarget(String),
    InvalidImage(String),
    Io(std::io::Error),
    Json(serde_json::Error),
}

impl fmt::Display for InstallerError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidArgument(message) => write!(f, "invalid argument: {message}"),
            Self::UnsafeTarget(message) => write!(f, "unsafe target: {message}"),
            Self::InvalidImage(message) => write!(f, "invalid image: {message}"),
            Self::Io(error) => write!(f, "I/O error: {error}"),
            Self::Json(error) => write!(f, "JSON error: {error}"),
        }
    }
}

impl std::error::Error for InstallerError {}

impl From<std::io::Error> for InstallerError {
    fn from(value: std::io::Error) -> Self {
        Self::Io(value)
    }
}

impl From<serde_json::Error> for InstallerError {
    fn from(value: serde_json::Error) -> Self {
        Self::Json(value)
    }
}

pub type Result<T> = std::result::Result<T, InstallerError>;

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct PartitionInfo {
    pub table_index: u8,
    pub partition_type: u8,
    pub start_sector: u64,
    pub sector_count: u64,
    pub end_sector_exclusive: u64,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct Fat32Info {
    pub bytes_per_sector: u32,
    pub sectors_per_cluster: u32,
    pub reserved_sectors: u32,
    pub fat_count: u8,
    pub sectors_per_fat: u32,
    pub declared_total_sectors: u64,
    pub data_start_sector_relative: u64,
    pub cluster_count: u64,
    pub fsinfo_sector_relative: u32,
    pub fsinfo_valid: bool,
    pub free_clusters: Option<u64>,
    pub estimated_used_clusters: Option<u64>,
    pub estimated_minimum_sectors_with_margin: Option<u64>,
    pub volume_id: u32,
    pub volume_label: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct Region {
    pub name: String,
    pub offset_bytes: u64,
    pub length_bytes: u64,
    pub writable_at_runtime: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct AndroidLayout {
    pub partition_start_sector: u64,
    pub partition_sector_count: u64,
    pub partition_size_bytes: u64,
    pub alignment_bytes: u64,
    pub regions: Vec<Region>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct SafetyState {
    pub target_kind: String,
    pub regular_file_only: bool,
    pub hardware_writes_enabled: bool,
    pub filesystem_resize_required: bool,
    pub filesystem_pre_shrunk: bool,
    pub image_layout_installed: bool,
    pub image_layout_matches_plan: bool,
    pub ready_for_image_layout_commit: bool,
    pub reasons: Vec<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct InstallPlan {
    pub protocol: u32,
    pub event: String,
    pub helper_version: String,
    pub hardware_writes_enabled: bool,
    pub plan_digest: String,
    pub image_path: String,
    pub image_size_bytes: u64,
    pub logical_sector_size: u32,
    pub image_identity_sha256: String,
    pub mbr_sector_sha256: String,
    pub fat32_boot_sector_sha256: String,
    pub fat32_fsinfo_sector_sha256: String,
    pub existing_partitions: Vec<PartitionInfo>,
    pub rockbox_partition: PartitionInfo,
    pub existing_android_partition: Option<PartitionInfo>,
    pub planned_rockbox_partition: PartitionInfo,
    pub fat32: Fat32Info,
    pub android: AndroidLayout,
    pub safety: SafetyState,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct HelloEvent {
    pub protocol: u32,
    pub event: String,
    pub helper_version: String,
    pub features: Vec<String>,
    pub hardware_writes_enabled: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct ImageTransactionRecord {
    pub protocol: u32,
    pub format: String,
    pub plan_digest_before: String,
    pub image_identity_before: String,
    pub image_size_bytes: u64,
    pub logical_sector_size: u32,
    pub android_start_sector: u64,
    pub android_sector_count: u64,
    pub original_mbr_sha256: String,
    pub committed_mbr_sha256: String,
    pub original_logical_sector_hex: String,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct ImageTransactionEvent {
    pub protocol: u32,
    pub event: String,
    pub helper_version: String,
    pub image_path: String,
    pub plan_digest_before: String,
    pub original_mbr_sha256: String,
    pub committed_mbr_sha256: String,
    pub metadata_verified: bool,
    pub mbr_verified: bool,
    pub hardware_writes_enabled: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ImageCommitFault {
    None,
    AfterMetadataSync,
    AfterMbrSync,
}

impl HelloEvent {
    pub fn current() -> Self {
        Self {
            protocol: PROTOCOL_VERSION,
            event: "hello".to_owned(),
            helper_version: HELPER_VERSION.to_owned(),
            features: vec![
                "regular-file-only".to_owned(),
                "mbr-read".to_owned(),
                "fat32-read".to_owned(),
                "dry-run-plan".to_owned(),
                "4k-sector-fixture".to_owned(),
                "image-layout-commit".to_owned(),
                "image-layout-rollback".to_owned(),
            ],
            hardware_writes_enabled: false,
        }
    }
}

pub fn commit_image_layout(
    path: &Path,
    sector_size: u32,
    android_size_mib: u64,
    expected_plan_digest: &str,
) -> Result<ImageTransactionEvent> {
    commit_image_layout_with_fault(
        path,
        sector_size,
        android_size_mib,
        expected_plan_digest,
        ImageCommitFault::None,
    )
}

pub fn commit_image_layout_with_fault(
    path: &Path,
    sector_size: u32,
    android_size_mib: u64,
    expected_plan_digest: &str,
    fault: ImageCommitFault,
) -> Result<ImageTransactionEvent> {
    let plan = plan_image(path, sector_size, android_size_mib)?;
    if plan.plan_digest != expected_plan_digest {
        return Err(InstallerError::UnsafeTarget(
            "plan digest changed; run a fresh dry run before committing".to_owned(),
        ));
    }
    if !plan.safety.ready_for_image_layout_commit {
        return Err(InstallerError::UnsafeTarget(format!(
            "image did not reach the layout-commit checkpoint: {}",
            plan.safety.reasons.join("; ")
        )));
    }

    let mut file = open_regular_file(path, true)?;
    let original = read_exact_at(&mut file, 0, usize::try_from(sector_size).unwrap())?;
    if sha256_hex(&original) != plan.mbr_sector_sha256 {
        return Err(InstallerError::UnsafeTarget(
            "MBR changed after planning".to_owned(),
        ));
    }
    let committed = build_committed_mbr(&original, &plan)?;
    let record = ImageTransactionRecord {
        protocol: PROTOCOL_VERSION,
        format: "rockpod-ipod6g-image-transaction-v1".to_owned(),
        plan_digest_before: plan.plan_digest.clone(),
        image_identity_before: plan.image_identity_sha256.clone(),
        image_size_bytes: plan.image_size_bytes,
        logical_sector_size: sector_size,
        android_start_sector: plan.android.partition_start_sector,
        android_sector_count: plan.android.partition_sector_count,
        original_mbr_sha256: sha256_hex(&original),
        committed_mbr_sha256: sha256_hex(&committed),
        original_logical_sector_hex: hex_encode(&original),
    };
    let metadata = encode_transaction_metadata(&record)?;
    let metadata_offset = record.android_start_sector * u64::from(sector_size);
    write_all_at(&mut file, metadata_offset, &metadata)?;
    file.sync_all()?;
    let verified_record = read_transaction_record(&mut file, metadata_offset)?;
    if verified_record != record {
        return Err(InstallerError::Io(std::io::Error::other(
            "transaction metadata read-back mismatch",
        )));
    }
    if fault == ImageCommitFault::AfterMetadataSync {
        return Err(InstallerError::Io(std::io::Error::other(
            "injected failure after transaction metadata sync",
        )));
    }

    write_all_at(&mut file, 0, &committed)?;
    file.sync_all()?;
    if fault == ImageCommitFault::AfterMbrSync {
        return Err(InstallerError::Io(std::io::Error::other(
            "injected failure after MBR sync",
        )));
    }
    let verify_mbr = read_exact_at(&mut file, 0, usize::try_from(sector_size).unwrap())?;
    if sha256_hex(&verify_mbr) != record.committed_mbr_sha256 {
        return Err(InstallerError::Io(std::io::Error::other(
            "committed MBR read-back mismatch",
        )));
    }
    let verify_plan = plan_image(path, sector_size, android_size_mib)?;
    if !verify_plan.safety.image_layout_installed || !verify_plan.safety.image_layout_matches_plan {
        return Err(InstallerError::Io(std::io::Error::other(
            "committed layout did not pass structural verification",
        )));
    }
    Ok(transaction_event("image-layout-committed", path, &record))
}

pub fn verify_image_layout(
    path: &Path,
    sector_size: u32,
    android_size_mib: u64,
) -> Result<ImageTransactionEvent> {
    let plan = plan_image(path, sector_size, android_size_mib)?;
    if !plan.safety.image_layout_installed || !plan.safety.image_layout_matches_plan {
        return Err(InstallerError::UnsafeTarget(
            "image does not contain the expected committed Android layout".to_owned(),
        ));
    }
    let android = plan.existing_android_partition.as_ref().unwrap();
    let mut file = open_regular_file(path, false)?;
    let record = read_transaction_record(&mut file, android.start_sector * u64::from(sector_size))?;
    validate_record_against_image(&record, &plan)?;
    Ok(transaction_event("image-layout-verified", path, &record))
}

pub fn rollback_image_layout(
    path: &Path,
    sector_size: u32,
    android_size_mib: u64,
) -> Result<ImageTransactionEvent> {
    let plan = plan_image(path, sector_size, android_size_mib)?;
    if !plan.safety.image_layout_installed || !plan.safety.image_layout_matches_plan {
        return Err(InstallerError::UnsafeTarget(
            "image does not contain the expected committed Android layout".to_owned(),
        ));
    }
    let android = plan.existing_android_partition.as_ref().unwrap();
    let mut file = open_regular_file(path, true)?;
    let metadata_offset = android.start_sector * u64::from(sector_size);
    let record = read_transaction_record(&mut file, metadata_offset)?;
    validate_record_against_image(&record, &plan)?;
    let original = hex_decode(&record.original_logical_sector_hex)?;
    if original.len() != usize::try_from(sector_size).unwrap()
        || sha256_hex(&original) != record.original_mbr_sha256
    {
        return Err(InstallerError::InvalidImage(
            "transaction record contains an invalid original MBR".to_owned(),
        ));
    }
    write_all_at(&mut file, 0, &original)?;
    file.sync_all()?;
    let verify = read_exact_at(&mut file, 0, original.len())?;
    if sha256_hex(&verify) != record.original_mbr_sha256 {
        return Err(InstallerError::Io(std::io::Error::other(
            "rolled-back MBR read-back mismatch",
        )));
    }
    let rollback_plan = plan_image(path, sector_size, android_size_mib)?;
    if rollback_plan.safety.image_layout_installed
        || !rollback_plan.safety.ready_for_image_layout_commit
    {
        return Err(InstallerError::Io(std::io::Error::other(
            "rolled-back image did not return to its qualified checkpoint",
        )));
    }
    Ok(transaction_event("image-layout-rolled-back", path, &record))
}

pub fn plan_image(path: &Path, sector_size: u32, android_size_mib: u64) -> Result<InstallPlan> {
    validate_sector_size(sector_size)?;
    if android_size_mib < MIN_ANDROID_MIB {
        return Err(InstallerError::InvalidArgument(format!(
            "Android allocation must be at least {MIN_ANDROID_MIB} MiB"
        )));
    }
    let mut file = open_regular_file(path, false)?;
    let metadata = file.metadata()?;
    let image_size = metadata.len();
    if image_size == 0 || image_size % u64::from(sector_size) != 0 {
        return Err(InstallerError::InvalidImage(format!(
            "image length {image_size} is not a non-zero multiple of sector size {sector_size}"
        )));
    }
    let total_sectors = image_size / u64::from(sector_size);
    if total_sectors > u64::from(u32::MAX) {
        return Err(InstallerError::InvalidImage(
            "DOS/MBR image exceeds the 32-bit LBA field".to_owned(),
        ));
    }

    let first_sector = read_exact_at(&mut file, 0, usize::try_from(sector_size).unwrap())?;
    let partitions = parse_mbr(&first_sector, total_sectors)?;
    let (rockbox, existing_android) = select_supported_layout(&partitions, total_sectors)?;
    let fat32 = parse_fat32(&mut file, &rockbox, sector_size, total_sectors)?;
    let fat32_partition_offset = rockbox
        .start_sector
        .checked_mul(u64::from(sector_size))
        .ok_or_else(|| InstallerError::InvalidImage("FAT32 byte offset overflow".to_owned()))?;
    let fat32_boot_sector = read_exact_at(
        &mut file,
        fat32_partition_offset,
        usize::try_from(sector_size).unwrap(),
    )?;
    let fat32_fsinfo_sector = read_exact_at(
        &mut file,
        fat32_partition_offset + u64::from(fat32.fsinfo_sector_relative) * u64::from(sector_size),
        usize::try_from(sector_size).unwrap(),
    )?;
    let android = build_android_layout(
        total_sectors,
        sector_size,
        android_size_mib.checked_mul(MIB).ok_or_else(|| {
            InstallerError::InvalidArgument("Android allocation overflow".to_owned())
        })?,
    )?;

    if android.partition_start_sector <= rockbox.start_sector {
        return Err(InstallerError::InvalidImage(
            "Android allocation would consume the FAT32 partition start".to_owned(),
        ));
    }
    let planned_fat_sectors = android.partition_start_sector - rockbox.start_sector;
    let planned_rockbox = PartitionInfo {
        table_index: rockbox.table_index,
        partition_type: rockbox.partition_type,
        start_sector: rockbox.start_sector,
        sector_count: planned_fat_sectors,
        end_sector_exclusive: android.partition_start_sector,
    };
    let image_layout_installed = existing_android.is_some();
    let image_layout_matches_plan = existing_android.as_ref().is_some_and(|partition| {
        partition.start_sector == android.partition_start_sector
            && partition.sector_count == android.partition_sector_count
            && rockbox == planned_rockbox
    });

    let filesystem_resize_required = fat32.declared_total_sectors > planned_fat_sectors;
    let filesystem_pre_shrunk = fat32.declared_total_sectors <= planned_fat_sectors;
    let mut reasons = Vec::new();
    if filesystem_resize_required {
        reasons.push(
            "FAT32 still declares sectors inside the proposed Android range; a qualified filesystem shrink is required"
                .to_owned(),
        );
    }
    if let Some(minimum) = fat32.estimated_minimum_sectors_with_margin {
        if planned_fat_sectors < minimum {
            reasons.push(format!(
                "planned FAT32 size {planned_fat_sectors} sectors is below the conservative estimated minimum {minimum}"
            ));
        }
    } else {
        reasons.push(
            "FAT32 FSInfo does not contain a trustworthy free-cluster count; shrink eligibility is unknown"
                .to_owned(),
        );
    }
    for partition in &partitions {
        if partition.table_index != rockbox.table_index
            && existing_android.as_ref() != Some(partition)
            && ranges_overlap(
                android.partition_start_sector,
                android.partition_start_sector + android.partition_sector_count,
                partition.start_sector,
                partition.end_sector_exclusive,
            )
        {
            reasons.push(format!(
                "Android range overlaps existing MBR entry {}",
                partition.table_index
            ));
        }
    }
    if image_layout_installed && !image_layout_matches_plan {
        reasons.push(
            "existing Android container does not match the requested allocation and FAT32 boundary"
                .to_owned(),
        );
    }
    let minimum_ok = fat32
        .estimated_minimum_sectors_with_margin
        .is_some_and(|minimum| planned_fat_sectors >= minimum);
    let ready =
        !image_layout_installed && filesystem_pre_shrunk && minimum_ok && reasons.is_empty();

    let identity = image_identity_digest(
        image_size,
        sector_size,
        &first_sector,
        &fat32_boot_sector,
        &fat32_fsinfo_sector,
    );
    let mut plan = InstallPlan {
        protocol: PROTOCOL_VERSION,
        event: "plan".to_owned(),
        helper_version: HELPER_VERSION.to_owned(),
        hardware_writes_enabled: false,
        plan_digest: String::new(),
        image_path: canonical_display_path(path),
        image_size_bytes: image_size,
        logical_sector_size: sector_size,
        image_identity_sha256: identity,
        mbr_sector_sha256: sha256_hex(&first_sector),
        fat32_boot_sector_sha256: sha256_hex(&fat32_boot_sector),
        fat32_fsinfo_sector_sha256: sha256_hex(&fat32_fsinfo_sector),
        existing_partitions: partitions,
        rockbox_partition: rockbox,
        existing_android_partition: existing_android,
        planned_rockbox_partition: planned_rockbox,
        fat32,
        android,
        safety: SafetyState {
            target_kind: "regular-file-image".to_owned(),
            regular_file_only: true,
            hardware_writes_enabled: false,
            filesystem_resize_required,
            filesystem_pre_shrunk,
            image_layout_installed,
            image_layout_matches_plan,
            ready_for_image_layout_commit: ready,
            reasons,
        },
    };
    plan.plan_digest = digest_plan(&plan)?;
    Ok(plan)
}

pub fn create_fixture(
    path: &Path,
    sector_size: u32,
    size_mib: u64,
    android_size_mib: u64,
    used_mib: u64,
    pre_shrunk: bool,
) -> Result<()> {
    validate_sector_size(sector_size)?;
    if size_mib < 2048 {
        return Err(InstallerError::InvalidArgument(
            "fixture image must be at least 2048 MiB so it is unambiguously FAT32".to_owned(),
        ));
    }
    if android_size_mib < MIN_ANDROID_MIB || android_size_mib + 512 >= size_mib {
        return Err(InstallerError::InvalidArgument(
            "fixture Android allocation leaves insufficient FAT32 space".to_owned(),
        ));
    }
    if used_mib + FAT_MARGIN_BYTES / MIB >= size_mib - android_size_mib {
        return Err(InstallerError::InvalidArgument(
            "fixture used space plus safety margin does not fit planned FAT32 size".to_owned(),
        ));
    }
    if path.exists() {
        return Err(InstallerError::UnsafeTarget(format!(
            "refusing to overwrite existing fixture {}",
            path.display()
        )));
    }
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let image_size = size_mib
        .checked_mul(MIB)
        .ok_or_else(|| InstallerError::InvalidArgument("fixture size overflow".to_owned()))?;
    let total_sectors = image_size / u64::from(sector_size);
    if total_sectors > u64::from(u32::MAX) {
        return Err(InstallerError::InvalidArgument(
            "fixture exceeds DOS/MBR sector fields".to_owned(),
        ));
    }
    let alignment_sectors = ALIGNMENT_BYTES / u64::from(sector_size);
    let partition_start = alignment_sectors;
    let partition_count = total_sectors - partition_start;
    let android_sectors = android_size_mib * MIB / u64::from(sector_size);
    let declared_total = if pre_shrunk {
        partition_count - android_sectors
    } else {
        partition_count
    };

    let mut file = OpenOptions::new()
        .create_new(true)
        .read(true)
        .write(true)
        .open(path)?;
    file.set_len(image_size)?;

    let mut mbr = vec![0u8; usize::try_from(sector_size).unwrap()];
    let entry = &mut mbr[MBR_PARTITION_OFFSET..MBR_PARTITION_OFFSET + MBR_PARTITION_SIZE];
    entry[4] = 0x0b;
    put_u32(entry, 8, to_u32(partition_start, "partition start")?);
    put_u32(entry, 12, to_u32(partition_count, "partition length")?);
    mbr[MBR_SIGNATURE_OFFSET] = 0x55;
    mbr[MBR_SIGNATURE_OFFSET + 1] = 0xaa;
    write_all_at(&mut file, 0, &mbr)?;

    let sectors_per_cluster = 8u32;
    let reserved_sectors = 32u32;
    let fat_count = 2u8;
    let sectors_per_fat = calculate_fat_sectors(
        declared_total,
        reserved_sectors,
        fat_count,
        sectors_per_cluster,
        sector_size,
    )?;
    let data_start =
        u64::from(reserved_sectors) + u64::from(fat_count) * u64::from(sectors_per_fat);
    let cluster_count = (declared_total - data_start) / u64::from(sectors_per_cluster);
    if cluster_count < 65_525 {
        return Err(InstallerError::InvalidArgument(
            "fixture geometry does not produce a FAT32 cluster count".to_owned(),
        ));
    }
    let requested_used_clusters =
        used_mib * MIB / (u64::from(sector_size) * u64::from(sectors_per_cluster));
    let used_clusters = requested_used_clusters.max(1).min(cluster_count - 1);
    let free_clusters = cluster_count - used_clusters;
    let volume_id = 0x524f_434bu32;

    let boot = make_fat32_boot_sector(
        sector_size,
        sectors_per_cluster,
        reserved_sectors,
        fat_count,
        sectors_per_fat,
        declared_total,
        partition_start,
        volume_id,
    )?;
    let partition_offset = partition_start * u64::from(sector_size);
    write_all_at(&mut file, partition_offset, &boot)?;
    write_all_at(
        &mut file,
        partition_offset + 6 * u64::from(sector_size),
        &boot,
    )?;

    let fsinfo = make_fsinfo_sector(sector_size, free_clusters)?;
    write_all_at(
        &mut file,
        partition_offset + u64::from(sector_size),
        &fsinfo,
    )?;
    write_all_at(
        &mut file,
        partition_offset + 7 * u64::from(sector_size),
        &fsinfo,
    )?;

    let fat_bytes = (used_clusters + 2)
        .checked_mul(4)
        .ok_or_else(|| InstallerError::InvalidArgument("fixture FAT overflow".to_owned()))?;
    let fat_written_sectors = div_ceil(fat_bytes, u64::from(sector_size));
    let mut fat = vec![0u8; usize::try_from(fat_written_sectors * u64::from(sector_size)).unwrap()];
    put_u32(&mut fat, 0, 0x0fff_fff8);
    put_u32(&mut fat, 4, 0x0fff_ffff);
    for cluster in 0..used_clusters {
        let offset = usize::try_from((cluster + 2) * 4).unwrap();
        let value = if cluster + 1 == used_clusters {
            0x0fff_ffff
        } else {
            u32::try_from(cluster + 3).unwrap()
        };
        put_u32(&mut fat, offset, value);
    }
    let fat1_offset = partition_offset + u64::from(reserved_sectors) * u64::from(sector_size);
    let fat2_offset = fat1_offset + u64::from(sectors_per_fat) * u64::from(sector_size);
    write_all_at(&mut file, fat1_offset, &fat)?;
    write_all_at(&mut file, fat2_offset, &fat)?;
    file.sync_all()?;
    Ok(())
}

fn build_committed_mbr(original: &[u8], plan: &InstallPlan) -> Result<Vec<u8>> {
    if plan.rockbox_partition.table_index != 1 || plan.existing_partitions.len() != 1 {
        return Err(InstallerError::UnsafeTarget(
            "image commit requires the observed FAT32 partition in MBR entry 1".to_owned(),
        ));
    }
    let mut committed = original.to_vec();
    let rockbox_entry =
        &mut committed[MBR_PARTITION_OFFSET..MBR_PARTITION_OFFSET + MBR_PARTITION_SIZE];
    put_u32(
        rockbox_entry,
        12,
        to_u32(
            plan.planned_rockbox_partition.sector_count,
            "planned FAT32 length",
        )?,
    );
    let android_offset = MBR_PARTITION_OFFSET + MBR_PARTITION_SIZE;
    let android_entry = &mut committed[android_offset..android_offset + MBR_PARTITION_SIZE];
    android_entry.fill(0);
    android_entry[4] = 0x83;
    put_u32(
        android_entry,
        8,
        to_u32(plan.android.partition_start_sector, "Android start")?,
    );
    put_u32(
        android_entry,
        12,
        to_u32(plan.android.partition_sector_count, "Android length")?,
    );
    Ok(committed)
}

fn encode_transaction_metadata(record: &ImageTransactionRecord) -> Result<Vec<u8>> {
    let body = serde_json::to_vec(record)?;
    let maximum = usize::try_from(ALIGNMENT_BYTES).unwrap() - TRANSACTION_HEADER_BYTES;
    if body.len() > maximum {
        return Err(InstallerError::InvalidArgument(
            "transaction record exceeds primary metadata region".to_owned(),
        ));
    }
    let mut metadata = vec![0u8; usize::try_from(ALIGNMENT_BYTES).unwrap()];
    metadata[..TRANSACTION_MAGIC.len()].copy_from_slice(TRANSACTION_MAGIC);
    put_u32(
        &mut metadata,
        32,
        u32::try_from(body.len()).map_err(|_| {
            InstallerError::InvalidArgument("transaction record length overflow".to_owned())
        })?,
    );
    metadata[36..68].copy_from_slice(&Sha256::digest(&body));
    metadata[TRANSACTION_HEADER_BYTES..TRANSACTION_HEADER_BYTES + body.len()]
        .copy_from_slice(&body);
    Ok(metadata)
}

fn read_transaction_record(file: &mut File, offset: u64) -> Result<ImageTransactionRecord> {
    let header = read_exact_at(file, offset, TRANSACTION_HEADER_BYTES)?;
    if !header.starts_with(TRANSACTION_MAGIC) {
        return Err(InstallerError::InvalidImage(
            "Android container has no Rockpod transaction metadata".to_owned(),
        ));
    }
    let body_len = usize::try_from(get_u32(&header, 32)).unwrap();
    if body_len == 0
        || body_len > usize::try_from(ALIGNMENT_BYTES).unwrap() - TRANSACTION_HEADER_BYTES
    {
        return Err(InstallerError::InvalidImage(
            "transaction metadata length is invalid".to_owned(),
        ));
    }
    let body = read_exact_at(
        file,
        offset + u64::try_from(TRANSACTION_HEADER_BYTES).unwrap(),
        body_len,
    )?;
    if header[36..68] != Sha256::digest(&body)[..] {
        return Err(InstallerError::InvalidImage(
            "transaction metadata checksum mismatch".to_owned(),
        ));
    }
    let record: ImageTransactionRecord = serde_json::from_slice(&body)?;
    if record.protocol != PROTOCOL_VERSION || record.format != "rockpod-ipod6g-image-transaction-v1"
    {
        return Err(InstallerError::InvalidImage(
            "unsupported transaction metadata format".to_owned(),
        ));
    }
    Ok(record)
}

fn validate_record_against_image(
    record: &ImageTransactionRecord,
    plan: &InstallPlan,
) -> Result<()> {
    if record.image_size_bytes != plan.image_size_bytes
        || record.logical_sector_size != plan.logical_sector_size
        || record.android_start_sector != plan.android.partition_start_sector
        || record.android_sector_count != plan.android.partition_sector_count
        || record.committed_mbr_sha256 != plan.mbr_sector_sha256
    {
        return Err(InstallerError::UnsafeTarget(
            "transaction metadata is not bound to the current image layout".to_owned(),
        ));
    }
    Ok(())
}

fn transaction_event(
    event: &str,
    path: &Path,
    record: &ImageTransactionRecord,
) -> ImageTransactionEvent {
    ImageTransactionEvent {
        protocol: PROTOCOL_VERSION,
        event: event.to_owned(),
        helper_version: HELPER_VERSION.to_owned(),
        image_path: canonical_display_path(path),
        plan_digest_before: record.plan_digest_before.clone(),
        original_mbr_sha256: record.original_mbr_sha256.clone(),
        committed_mbr_sha256: record.committed_mbr_sha256.clone(),
        metadata_verified: true,
        mbr_verified: true,
        hardware_writes_enabled: false,
    }
}

fn hex_encode(bytes: &[u8]) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut output = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        output.push(char::from(HEX[usize::from(byte >> 4)]));
        output.push(char::from(HEX[usize::from(byte & 0x0f)]));
    }
    output
}

fn hex_decode(text: &str) -> Result<Vec<u8>> {
    if !text.len().is_multiple_of(2) {
        return Err(InstallerError::InvalidImage(
            "transaction MBR hex has an odd length".to_owned(),
        ));
    }
    text.as_bytes()
        .chunks_exact(2)
        .map(|pair| {
            let high = hex_nibble(pair[0])?;
            let low = hex_nibble(pair[1])?;
            Ok((high << 4) | low)
        })
        .collect()
}

fn hex_nibble(byte: u8) -> Result<u8> {
    match byte {
        b'0'..=b'9' => Ok(byte - b'0'),
        b'a'..=b'f' => Ok(byte - b'a' + 10),
        b'A'..=b'F' => Ok(byte - b'A' + 10),
        _ => Err(InstallerError::InvalidImage(
            "transaction MBR hex contains a non-hex character".to_owned(),
        )),
    }
}

fn parse_mbr(sector: &[u8], total_sectors: u64) -> Result<Vec<PartitionInfo>> {
    if sector.len() < 512 {
        return Err(InstallerError::InvalidImage(
            "logical sector is shorter than the DOS MBR".to_owned(),
        ));
    }
    if sector[MBR_SIGNATURE_OFFSET] != 0x55 || sector[MBR_SIGNATURE_OFFSET + 1] != 0xaa {
        return Err(InstallerError::InvalidImage(
            "missing DOS MBR 0x55aa signature".to_owned(),
        ));
    }
    let mut partitions = Vec::new();
    for index in 0..4usize {
        let offset = MBR_PARTITION_OFFSET + index * MBR_PARTITION_SIZE;
        let entry = &sector[offset..offset + MBR_PARTITION_SIZE];
        let partition_type = entry[4];
        let start = u64::from(get_u32(entry, 8));
        let count = u64::from(get_u32(entry, 12));
        if partition_type == 0 && start == 0 && count == 0 {
            continue;
        }
        if partition_type == 0 || count == 0 {
            return Err(InstallerError::InvalidImage(format!(
                "MBR entry {} is only partially empty",
                index + 1
            )));
        }
        let end = start.checked_add(count).ok_or_else(|| {
            InstallerError::InvalidImage(format!("MBR entry {} overflows", index + 1))
        })?;
        if start == 0 || end > total_sectors {
            return Err(InstallerError::InvalidImage(format!(
                "MBR entry {} lies outside the image",
                index + 1
            )));
        }
        partitions.push(PartitionInfo {
            table_index: u8::try_from(index + 1).unwrap(),
            partition_type,
            start_sector: start,
            sector_count: count,
            end_sector_exclusive: end,
        });
    }
    for left in 0..partitions.len() {
        for right in (left + 1)..partitions.len() {
            if ranges_overlap(
                partitions[left].start_sector,
                partitions[left].end_sector_exclusive,
                partitions[right].start_sector,
                partitions[right].end_sector_exclusive,
            ) {
                return Err(InstallerError::InvalidImage(format!(
                    "MBR entries {} and {} overlap",
                    partitions[left].table_index, partitions[right].table_index
                )));
            }
        }
    }
    Ok(partitions)
}

fn select_supported_layout(
    partitions: &[PartitionInfo],
    total_sectors: u64,
) -> Result<(PartitionInfo, Option<PartitionInfo>)> {
    let fat: Vec<_> = partitions
        .iter()
        .filter(|partition| FAT32_TYPES.contains(&partition.partition_type))
        .cloned()
        .collect();
    if fat.len() != 1 {
        return Err(InstallerError::InvalidImage(format!(
            "expected exactly one FAT32 MBR partition, found {}",
            fat.len()
        )));
    }
    if partitions.len() > 2 {
        return Err(InstallerError::InvalidImage(
            "Phase 0 planner supports only FAT32 plus one Android container".to_owned(),
        ));
    }
    let rockbox = fat[0].clone();
    let android = partitions
        .iter()
        .find(|partition| partition.table_index != rockbox.table_index)
        .cloned();
    if let Some(partition) = &android
        && (partition.partition_type != 0x83
            || partition.start_sector != rockbox.end_sector_exclusive
            || partition.end_sector_exclusive != total_sectors)
    {
        return Err(InstallerError::InvalidImage(
            "second MBR entry is not the expected contiguous trailing Android container".to_owned(),
        ));
    }
    Ok((rockbox, android))
}

fn parse_fat32(
    file: &mut File,
    partition: &PartitionInfo,
    sector_size: u32,
    disk_sectors: u64,
) -> Result<Fat32Info> {
    let offset = partition
        .start_sector
        .checked_mul(u64::from(sector_size))
        .ok_or_else(|| InstallerError::InvalidImage("FAT32 byte offset overflow".to_owned()))?;
    let boot = read_exact_at(file, offset, usize::try_from(sector_size).unwrap())?;
    if boot[MBR_SIGNATURE_OFFSET] != 0x55 || boot[MBR_SIGNATURE_OFFSET + 1] != 0xaa {
        return Err(InstallerError::InvalidImage(
            "FAT32 boot sector is missing its signature".to_owned(),
        ));
    }
    let bytes_per_sector = u32::from(get_u16(&boot, 11));
    if bytes_per_sector != sector_size {
        return Err(InstallerError::InvalidImage(format!(
            "FAT32 bytes/sector {bytes_per_sector} differs from requested logical sector {sector_size}"
        )));
    }
    let sectors_per_cluster = u32::from(boot[13]);
    if sectors_per_cluster == 0 || !sectors_per_cluster.is_power_of_two() {
        return Err(InstallerError::InvalidImage(
            "FAT32 sectors/cluster is not a non-zero power of two".to_owned(),
        ));
    }
    let reserved_sectors = u32::from(get_u16(&boot, 14));
    let fat_count = boot[16];
    let root_entries = get_u16(&boot, 17);
    let total16 = u64::from(get_u16(&boot, 19));
    let fat16 = get_u16(&boot, 22);
    let hidden = u64::from(get_u32(&boot, 28));
    let total32 = u64::from(get_u32(&boot, 32));
    let sectors_per_fat = get_u32(&boot, 36);
    let fsinfo_sector = u32::from(get_u16(&boot, 48));
    let volume_id = get_u32(&boot, 67);
    let volume_label = clean_ascii(&boot[71..82]);
    if reserved_sectors == 0 || !(fat_count == 1 || fat_count == 2) {
        return Err(InstallerError::InvalidImage(
            "FAT32 reserved-sector or FAT-count fields are invalid".to_owned(),
        ));
    }
    if root_entries != 0 || fat16 != 0 || sectors_per_fat == 0 {
        return Err(InstallerError::InvalidImage(
            "volume does not have FAT32 BPB geometry".to_owned(),
        ));
    }
    if hidden != partition.start_sector {
        return Err(InstallerError::InvalidImage(format!(
            "FAT32 hidden-sector field {hidden} differs from MBR start {}",
            partition.start_sector
        )));
    }
    let declared_total = if total16 != 0 { total16 } else { total32 };
    if declared_total == 0 || declared_total > partition.sector_count {
        return Err(InstallerError::InvalidImage(format!(
            "FAT32 declared size {declared_total} exceeds MBR partition length {}",
            partition.sector_count
        )));
    }
    if partition.start_sector + declared_total > disk_sectors {
        return Err(InstallerError::InvalidImage(
            "FAT32 declared extent exceeds disk".to_owned(),
        ));
    }
    let data_start = u64::from(reserved_sectors)
        .checked_add(u64::from(fat_count) * u64::from(sectors_per_fat))
        .ok_or_else(|| InstallerError::InvalidImage("FAT32 metadata overflow".to_owned()))?;
    if data_start >= declared_total {
        return Err(InstallerError::InvalidImage(
            "FAT32 metadata consumes the declared volume".to_owned(),
        ));
    }
    let cluster_count = (declared_total - data_start) / u64::from(sectors_per_cluster);
    if cluster_count < 65_525 {
        return Err(InstallerError::InvalidImage(format!(
            "cluster count {cluster_count} is below the FAT32 threshold"
        )));
    }

    let fsinfo_offset = offset
        .checked_add(u64::from(fsinfo_sector) * u64::from(sector_size))
        .ok_or_else(|| InstallerError::InvalidImage("FSInfo offset overflow".to_owned()))?;
    let fsinfo = read_exact_at(file, fsinfo_offset, usize::try_from(sector_size).unwrap())?;
    let fsinfo_valid = get_u32(&fsinfo, 0) == 0x4161_5252
        && get_u32(&fsinfo, 484) == 0x6141_7272
        && get_u32(&fsinfo, 508) == 0xaa55_0000;
    let free_raw = get_u32(&fsinfo, 488);
    let free_clusters =
        if fsinfo_valid && free_raw != u32::MAX && u64::from(free_raw) <= cluster_count {
            Some(u64::from(free_raw))
        } else {
            None
        };
    let estimated_used = free_clusters.map(|free| cluster_count - free);
    let margin_sectors = FAT_MARGIN_BYTES / u64::from(sector_size);
    let estimated_minimum = estimated_used.and_then(|used| {
        data_start
            .checked_add(used.checked_mul(u64::from(sectors_per_cluster))?)?
            .checked_add(margin_sectors)
    });
    Ok(Fat32Info {
        bytes_per_sector,
        sectors_per_cluster,
        reserved_sectors,
        fat_count,
        sectors_per_fat,
        declared_total_sectors: declared_total,
        data_start_sector_relative: data_start,
        cluster_count,
        fsinfo_sector_relative: fsinfo_sector,
        fsinfo_valid,
        free_clusters,
        estimated_used_clusters: estimated_used,
        estimated_minimum_sectors_with_margin: estimated_minimum,
        volume_id,
        volume_label,
    })
}

fn build_android_layout(
    total_sectors: u64,
    sector_size: u32,
    requested_bytes: u64,
) -> Result<AndroidLayout> {
    let alignment_sectors = ALIGNMENT_BYTES / u64::from(sector_size);
    if alignment_sectors == 0 || !requested_bytes.is_multiple_of(ALIGNMENT_BYTES) {
        return Err(InstallerError::InvalidArgument(
            "Android allocation must be aligned to 1 MiB".to_owned(),
        ));
    }
    let requested_sectors = requested_bytes / u64::from(sector_size);
    if requested_sectors >= total_sectors {
        return Err(InstallerError::InvalidArgument(
            "Android allocation exceeds image".to_owned(),
        ));
    }
    let start = align_down(total_sectors - requested_sectors, alignment_sectors);
    let count = total_sectors - start;
    let size_bytes = count
        .checked_mul(u64::from(sector_size))
        .ok_or_else(|| InstallerError::InvalidArgument("Android layout overflow".to_owned()))?;

    let fixed = [
        ("primary_metadata", MIB, false),
        ("boot_a", 16 * MIB, false),
        ("boot_b", 16 * MIB, false),
        ("system_a", 96 * MIB, false),
        ("system_b", 96 * MIB, false),
    ];
    let backup_metadata = MIB;
    let fixed_total: u64 = fixed.iter().map(|(_, size, _)| *size).sum::<u64>() + backup_metadata;
    let minimum_data = 256 * MIB;
    if size_bytes < fixed_total + minimum_data {
        return Err(InstallerError::InvalidArgument(format!(
            "Android allocation {size_bytes} bytes leaves less than 256 MiB for data"
        )));
    }
    let mut regions = Vec::new();
    let mut offset = 0u64;
    for (name, length, writable) in fixed {
        regions.push(Region {
            name: name.to_owned(),
            offset_bytes: offset,
            length_bytes: length,
            writable_at_runtime: writable,
        });
        offset += length;
    }
    let data_length = size_bytes - offset - backup_metadata;
    regions.push(Region {
        name: "data".to_owned(),
        offset_bytes: offset,
        length_bytes: data_length,
        writable_at_runtime: true,
    });
    regions.push(Region {
        name: "backup_metadata".to_owned(),
        offset_bytes: offset + data_length,
        length_bytes: backup_metadata,
        writable_at_runtime: false,
    });
    validate_regions(&regions, size_bytes)?;
    Ok(AndroidLayout {
        partition_start_sector: start,
        partition_sector_count: count,
        partition_size_bytes: size_bytes,
        alignment_bytes: ALIGNMENT_BYTES,
        regions,
    })
}

fn validate_regions(regions: &[Region], partition_size: u64) -> Result<()> {
    let mut expected = 0u64;
    for region in regions {
        if region.offset_bytes != expected || region.offset_bytes % ALIGNMENT_BYTES != 0 {
            return Err(InstallerError::InvalidArgument(format!(
                "region {} is not contiguous and 1 MiB aligned",
                region.name
            )));
        }
        if region.length_bytes == 0 || region.length_bytes % ALIGNMENT_BYTES != 0 {
            return Err(InstallerError::InvalidArgument(format!(
                "region {} length is not positive and 1 MiB aligned",
                region.name
            )));
        }
        expected = expected
            .checked_add(region.length_bytes)
            .ok_or_else(|| InstallerError::InvalidArgument("Android region overflow".to_owned()))?;
    }
    if expected != partition_size {
        return Err(InstallerError::InvalidArgument(
            "Android regions do not exactly cover the partition".to_owned(),
        ));
    }
    Ok(())
}

fn calculate_fat_sectors(
    total_sectors: u64,
    reserved: u32,
    fat_count: u8,
    sectors_per_cluster: u32,
    bytes_per_sector: u32,
) -> Result<u32> {
    let mut fat_sectors = 1u64;
    for _ in 0..32 {
        let metadata = u64::from(reserved) + u64::from(fat_count) * fat_sectors;
        if metadata >= total_sectors {
            return Err(InstallerError::InvalidArgument(
                "FAT metadata exceeds fixture volume".to_owned(),
            ));
        }
        let clusters = (total_sectors - metadata) / u64::from(sectors_per_cluster);
        let needed = div_ceil((clusters + 2) * 4, u64::from(bytes_per_sector));
        if needed == fat_sectors {
            return to_u32(needed, "FAT sector count");
        }
        fat_sectors = needed;
    }
    Err(InstallerError::InvalidArgument(
        "FAT geometry did not converge".to_owned(),
    ))
}

#[allow(clippy::too_many_arguments)]
fn make_fat32_boot_sector(
    sector_size: u32,
    sectors_per_cluster: u32,
    reserved: u32,
    fat_count: u8,
    sectors_per_fat: u32,
    total_sectors: u64,
    hidden_sectors: u64,
    volume_id: u32,
) -> Result<Vec<u8>> {
    let mut boot = vec![0u8; usize::try_from(sector_size).unwrap()];
    boot[0..3].copy_from_slice(&[0xeb, 0x58, 0x90]);
    boot[3..11].copy_from_slice(b"MSWIN4.1");
    put_u16(
        &mut boot,
        11,
        u16::try_from(sector_size).map_err(|_| {
            InstallerError::InvalidArgument("FAT sector size exceeds BPB field".to_owned())
        })?,
    );
    boot[13] = u8::try_from(sectors_per_cluster).unwrap();
    put_u16(&mut boot, 14, u16::try_from(reserved).unwrap());
    boot[16] = fat_count;
    put_u16(&mut boot, 17, 0);
    put_u16(&mut boot, 19, 0);
    boot[21] = 0xf8;
    put_u16(&mut boot, 22, 0);
    put_u16(&mut boot, 24, 63);
    put_u16(&mut boot, 26, 255);
    put_u32(
        &mut boot,
        28,
        to_u32(hidden_sectors, "hidden sector count")?,
    );
    put_u32(&mut boot, 32, to_u32(total_sectors, "FAT32 total sectors")?);
    put_u32(&mut boot, 36, sectors_per_fat);
    put_u16(&mut boot, 40, 0);
    put_u16(&mut boot, 42, 0);
    put_u32(&mut boot, 44, 2);
    put_u16(&mut boot, 48, 1);
    put_u16(&mut boot, 50, 6);
    boot[64] = 0x80;
    boot[66] = 0x29;
    put_u32(&mut boot, 67, volume_id);
    boot[71..82].copy_from_slice(b"ROCKBOX    ");
    boot[82..90].copy_from_slice(b"FAT32   ");
    boot[MBR_SIGNATURE_OFFSET] = 0x55;
    boot[MBR_SIGNATURE_OFFSET + 1] = 0xaa;
    Ok(boot)
}

fn make_fsinfo_sector(sector_size: u32, free_clusters: u64) -> Result<Vec<u8>> {
    let mut info = vec![0u8; usize::try_from(sector_size).unwrap()];
    put_u32(&mut info, 0, 0x4161_5252);
    put_u32(&mut info, 484, 0x6141_7272);
    put_u32(&mut info, 488, to_u32(free_clusters, "free cluster count")?);
    put_u32(&mut info, 492, 3);
    put_u32(&mut info, 508, 0xaa55_0000);
    Ok(info)
}

fn require_regular_file(path: &Path) -> Result<()> {
    let metadata = fs::symlink_metadata(path).map_err(|error| {
        if error.kind() == std::io::ErrorKind::NotFound {
            InstallerError::InvalidArgument(format!("target does not exist: {}", path.display()))
        } else {
            InstallerError::Io(error)
        }
    })?;
    if metadata.file_type().is_symlink() || !metadata.file_type().is_file() {
        return Err(InstallerError::UnsafeTarget(format!(
            "Phase 0 accepts regular image files only: {}",
            path.display()
        )));
    }
    Ok(())
}

fn open_regular_file(path: &Path, write: bool) -> Result<File> {
    require_regular_file(path)?;
    let before = fs::symlink_metadata(path)?;
    let file = OpenOptions::new().read(true).write(write).open(path)?;
    let opened = file.metadata()?;
    if !opened.file_type().is_file() {
        return Err(InstallerError::UnsafeTarget(format!(
            "opened target is not a regular image file: {}",
            path.display()
        )));
    }
    #[cfg(unix)]
    if before.dev() != opened.dev() || before.ino() != opened.ino() {
        return Err(InstallerError::UnsafeTarget(format!(
            "target identity changed while it was being opened: {}",
            path.display()
        )));
    }
    Ok(file)
}

fn validate_sector_size(sector_size: u32) -> Result<()> {
    if !matches!(sector_size, 512 | 4096) {
        return Err(InstallerError::InvalidArgument(
            "logical sector size must be 512 or 4096 bytes".to_owned(),
        ));
    }
    Ok(())
}

fn digest_plan(plan: &InstallPlan) -> Result<String> {
    let bytes = serde_json::to_vec(plan)?;
    Ok(sha256_hex(&bytes))
}

fn image_identity_digest(
    image_size: u64,
    sector_size: u32,
    first_sector: &[u8],
    fat32_boot_sector: &[u8],
    fat32_fsinfo_sector: &[u8],
) -> String {
    let mut digest = Sha256::new();
    digest.update(b"rockpod-ipod6g-image-v1\0");
    digest.update(image_size.to_le_bytes());
    digest.update(sector_size.to_le_bytes());
    digest.update(first_sector);
    digest.update(fat32_boot_sector);
    digest.update(fat32_fsinfo_sector);
    format!("{:x}", digest.finalize())
}

fn sha256_hex(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

fn canonical_display_path(path: &Path) -> String {
    fs::canonicalize(path)
        .unwrap_or_else(|_| PathBuf::from(path))
        .to_string_lossy()
        .into_owned()
}

fn read_exact_at(file: &mut File, offset: u64, size: usize) -> Result<Vec<u8>> {
    file.seek(SeekFrom::Start(offset))?;
    let mut buffer = vec![0u8; size];
    file.read_exact(&mut buffer)?;
    Ok(buffer)
}

fn write_all_at(file: &mut File, offset: u64, bytes: &[u8]) -> Result<()> {
    file.seek(SeekFrom::Start(offset))?;
    file.write_all(bytes)?;
    Ok(())
}

fn get_u16(bytes: &[u8], offset: usize) -> u16 {
    u16::from_le_bytes([bytes[offset], bytes[offset + 1]])
}

fn get_u32(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes([
        bytes[offset],
        bytes[offset + 1],
        bytes[offset + 2],
        bytes[offset + 3],
    ])
}

fn put_u16(bytes: &mut [u8], offset: usize, value: u16) {
    bytes[offset..offset + 2].copy_from_slice(&value.to_le_bytes());
}

fn put_u32(bytes: &mut [u8], offset: usize, value: u32) {
    bytes[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
}

fn to_u32(value: u64, label: &str) -> Result<u32> {
    u32::try_from(value)
        .map_err(|_| InstallerError::InvalidArgument(format!("{label} exceeds a 32-bit field")))
}

fn align_down(value: u64, alignment: u64) -> u64 {
    value / alignment * alignment
}

fn div_ceil(value: u64, divisor: u64) -> u64 {
    value.div_ceil(divisor)
}

fn ranges_overlap(left_start: u64, left_end: u64, right_start: u64, right_end: u64) -> bool {
    left_start < right_end && right_start < left_end
}

fn clean_ascii(bytes: &[u8]) -> String {
    String::from_utf8_lossy(bytes)
        .trim_matches(|character: char| character == ' ' || character == '\0')
        .to_owned()
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    fn fixture(pre_shrunk: bool) -> (tempfile::TempDir, PathBuf) {
        let dir = tempdir().unwrap();
        let path = dir.path().join("ipod.img");
        create_fixture(&path, 4096, 4096, 1024, 256, pre_shrunk).unwrap();
        (dir, path)
    }

    #[test]
    fn unshrunk_fixture_requires_filesystem_resize() {
        let (_dir, path) = fixture(false);
        let plan = plan_image(&path, 4096, 1024).unwrap();
        assert!(plan.safety.filesystem_resize_required);
        assert!(!plan.safety.filesystem_pre_shrunk);
        assert!(!plan.safety.ready_for_image_layout_commit);
        assert!(!plan.safety.hardware_writes_enabled);
        assert_eq!(plan.safety.target_kind, "regular-file-image");
    }

    #[test]
    fn pre_shrunk_fixture_reaches_layout_commit_checkpoint() {
        let (_dir, path) = fixture(true);
        let plan = plan_image(&path, 4096, 1024).unwrap();
        assert!(!plan.safety.filesystem_resize_required);
        assert!(plan.safety.filesystem_pre_shrunk);
        assert!(plan.safety.ready_for_image_layout_commit);
        assert!(plan.safety.reasons.is_empty());
        assert_eq!(
            plan.planned_rockbox_partition.end_sector_exclusive,
            plan.android.partition_start_sector
        );
    }

    #[test]
    fn plan_digest_is_stable() {
        let (_dir, path) = fixture(true);
        let first = plan_image(&path, 4096, 1024).unwrap();
        let second = plan_image(&path, 4096, 1024).unwrap();
        assert_eq!(first.plan_digest, second.plan_digest);
        assert_eq!(first.image_identity_sha256, second.image_identity_sha256);
    }

    #[test]
    fn image_identity_binds_fat32_checkpoint_state() {
        let unshrunk_dir = tempdir().unwrap();
        let unshrunk = unshrunk_dir.path().join("unshrunk.img");
        create_fixture(&unshrunk, 4096, 4096, 1024, 256, false).unwrap();
        let preshrunk_dir = tempdir().unwrap();
        let preshrunk = preshrunk_dir.path().join("preshrunk.img");
        create_fixture(&preshrunk, 4096, 4096, 1024, 256, true).unwrap();

        let first = plan_image(&unshrunk, 4096, 1024).unwrap();
        let second = plan_image(&preshrunk, 4096, 1024).unwrap();
        assert_eq!(first.mbr_sector_sha256, second.mbr_sector_sha256);
        assert_ne!(
            first.fat32_boot_sector_sha256,
            second.fat32_boot_sector_sha256
        );
        assert_ne!(first.image_identity_sha256, second.image_identity_sha256);
    }

    #[test]
    fn regions_cover_partition_without_gaps() {
        let (_dir, path) = fixture(true);
        let plan = plan_image(&path, 4096, 1024).unwrap();
        let mut end = 0;
        for region in &plan.android.regions {
            assert_eq!(region.offset_bytes, end);
            assert_eq!(region.offset_bytes % ALIGNMENT_BYTES, 0);
            end += region.length_bytes;
        }
        assert_eq!(end, plan.android.partition_size_bytes);
        assert_eq!(plan.android.regions.last().unwrap().name, "backup_metadata");
    }

    #[test]
    fn rejects_non_regular_target() {
        let error = plan_image(Path::new("/dev/null"), 4096, 1024).unwrap_err();
        assert!(matches!(error, InstallerError::UnsafeTarget(_)));
    }

    #[cfg(unix)]
    #[test]
    fn rejects_symlink_to_regular_image() {
        use std::os::unix::fs::symlink;

        let (_dir, path) = fixture(false);
        let link = path.with_file_name("ipod-link.img");
        symlink(&path, &link).unwrap();

        let error = plan_image(&link, 4096, 1024).unwrap_err();
        assert!(matches!(error, InstallerError::UnsafeTarget(_)));
    }

    #[test]
    fn rejects_corrupt_mbr_signature() {
        let (_dir, path) = fixture(false);
        let mut file = OpenOptions::new().write(true).open(&path).unwrap();
        write_all_at(&mut file, 510, &[0, 0]).unwrap();
        let error = plan_image(&path, 4096, 1024).unwrap_err();
        assert!(error.to_string().contains("MBR"));
    }

    #[test]
    fn rejects_allocation_below_container_minimum() {
        let (_dir, path) = fixture(false);
        let error = plan_image(&path, 4096, 256).unwrap_err();
        assert!(error.to_string().contains("at least 512 MiB"));
    }

    #[test]
    fn image_layout_commit_verify_and_rollback_round_trip() {
        let (_dir, path) = fixture(true);
        let before = plan_image(&path, 4096, 1024).unwrap();

        let committed = commit_image_layout(&path, 4096, 1024, &before.plan_digest).unwrap();
        assert_eq!(committed.event, "image-layout-committed");
        assert!(committed.metadata_verified);
        assert!(committed.mbr_verified);
        let installed = plan_image(&path, 4096, 1024).unwrap();
        assert!(installed.safety.image_layout_installed);
        assert!(installed.safety.image_layout_matches_plan);
        assert!(!installed.safety.ready_for_image_layout_commit);
        assert_eq!(installed.existing_partitions.len(), 2);
        assert_eq!(
            installed.existing_android_partition.unwrap().partition_type,
            0x83
        );

        assert_eq!(
            verify_image_layout(&path, 4096, 1024).unwrap().event,
            "image-layout-verified"
        );
        let rolled_back = rollback_image_layout(&path, 4096, 1024).unwrap();
        assert_eq!(rolled_back.event, "image-layout-rolled-back");
        let after = plan_image(&path, 4096, 1024).unwrap();
        assert!(!after.safety.image_layout_installed);
        assert!(after.safety.ready_for_image_layout_commit);
        assert_eq!(after.mbr_sector_sha256, before.mbr_sector_sha256);
    }

    #[test]
    fn stale_plan_digest_prevents_image_commit() {
        let (_dir, path) = fixture(true);
        let error = commit_image_layout(&path, 4096, 1024, &"0".repeat(64)).unwrap_err();
        assert!(error.to_string().contains("plan digest changed"));
        assert!(
            plan_image(&path, 4096, 1024)
                .unwrap()
                .safety
                .ready_for_image_layout_commit
        );
    }

    #[test]
    fn failure_after_metadata_sync_leaves_original_mbr_active() {
        let (_dir, path) = fixture(true);
        let before = plan_image(&path, 4096, 1024).unwrap();
        let error = commit_image_layout_with_fault(
            &path,
            4096,
            1024,
            &before.plan_digest,
            ImageCommitFault::AfterMetadataSync,
        )
        .unwrap_err();
        assert!(error.to_string().contains("injected failure"));
        let after = plan_image(&path, 4096, 1024).unwrap();
        assert_eq!(after.mbr_sector_sha256, before.mbr_sector_sha256);
        assert!(!after.safety.image_layout_installed);
    }

    #[test]
    fn failure_after_mbr_sync_is_recoverable_from_container_metadata() {
        let (_dir, path) = fixture(true);
        let before = plan_image(&path, 4096, 1024).unwrap();
        let error = commit_image_layout_with_fault(
            &path,
            4096,
            1024,
            &before.plan_digest,
            ImageCommitFault::AfterMbrSync,
        )
        .unwrap_err();
        assert!(error.to_string().contains("injected failure"));
        assert_eq!(
            verify_image_layout(&path, 4096, 1024).unwrap().event,
            "image-layout-verified"
        );
        rollback_image_layout(&path, 4096, 1024).unwrap();
        assert!(
            plan_image(&path, 4096, 1024)
                .unwrap()
                .safety
                .ready_for_image_layout_commit
        );
    }

    #[test]
    fn corrupted_transaction_metadata_prevents_verify_and_rollback() {
        let (_dir, path) = fixture(true);
        let before = plan_image(&path, 4096, 1024).unwrap();
        commit_image_layout(&path, 4096, 1024, &before.plan_digest).unwrap();
        let installed = plan_image(&path, 4096, 1024).unwrap();
        let metadata_offset =
            installed.android.partition_start_sector * u64::from(installed.logical_sector_size);
        let mut file = OpenOptions::new()
            .read(true)
            .write(true)
            .open(&path)
            .unwrap();
        let original_byte = read_exact_at(&mut file, metadata_offset + 40, 1).unwrap()[0];
        write_all_at(&mut file, metadata_offset + 40, &[original_byte ^ 0xff]).unwrap();

        assert!(verify_image_layout(&path, 4096, 1024).is_err());
        assert!(rollback_image_layout(&path, 4096, 1024).is_err());
    }
}
