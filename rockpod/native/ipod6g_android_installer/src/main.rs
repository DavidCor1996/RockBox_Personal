use ipod6g_android_installer::{
    DEFAULT_ANDROID_MIB, HELPER_VERSION, HelloEvent, InstallerError, commit_image_layout,
    create_fixture, plan_image, rollback_image_layout, verify_image_layout,
};
use serde::Serialize;
use serde_json::json;
use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    if let Err(error) = run() {
        let event = json!({
            "protocol": 1,
            "event": "error",
            "helper_version": HELPER_VERSION,
            "code": error_code(&error),
            "message": error.to_string(),
            "hardware_writes_enabled": false,
        });
        println!("{}", serde_json::to_string(&event).unwrap());
        std::process::exit(2);
    }
}

fn run() -> Result<(), InstallerError> {
    let mut args = env::args().skip(1);
    let command = args.next().unwrap_or_else(|| "help".to_owned());
    let rest: Vec<String> = args.collect();
    match command.as_str() {
        "hello" => print_json(&HelloEvent::current()),
        "plan" | "dry-run" => {
            let image = required_value(&rest, "--image")?;
            let sector_size = value_or(&rest, "--sector-size", 4096u32)?;
            let android_size = value_or(&rest, "--android-size-mib", DEFAULT_ANDROID_MIB)?;
            let plan = plan_image(&PathBuf::from(image), sector_size, android_size)?;
            if let Some(output) = optional_value(&rest, "--output") {
                let bytes = serde_json::to_vec_pretty(&plan)?;
                fs::write(output, bytes)?;
            }
            print_json(&plan)
        }
        "commit-image-layout" => {
            let image = required_value(&rest, "--image")?;
            let sector_size = value_or(&rest, "--sector-size", 4096u32)?;
            let android_size = value_or(&rest, "--android-size-mib", DEFAULT_ANDROID_MIB)?;
            let expected_digest = required_value(&rest, "--expected-plan-digest")?;
            print_json(&commit_image_layout(
                &PathBuf::from(image),
                sector_size,
                android_size,
                &expected_digest,
            )?)
        }
        "verify-image-layout" => {
            let image = required_value(&rest, "--image")?;
            let sector_size = value_or(&rest, "--sector-size", 4096u32)?;
            let android_size = value_or(&rest, "--android-size-mib", DEFAULT_ANDROID_MIB)?;
            print_json(&verify_image_layout(
                &PathBuf::from(image),
                sector_size,
                android_size,
            )?)
        }
        "rollback-image-layout" => {
            let image = required_value(&rest, "--image")?;
            let sector_size = value_or(&rest, "--sector-size", 4096u32)?;
            let android_size = value_or(&rest, "--android-size-mib", DEFAULT_ANDROID_MIB)?;
            print_json(&rollback_image_layout(
                &PathBuf::from(image),
                sector_size,
                android_size,
            )?)
        }
        "create-fixture" => {
            let image = PathBuf::from(required_value(&rest, "--image")?);
            let sector_size = value_or(&rest, "--sector-size", 4096u32)?;
            let size_mib = value_or(&rest, "--size-mib", 4096u64)?;
            let android_size = value_or(&rest, "--android-size-mib", DEFAULT_ANDROID_MIB)?;
            let used_mib = value_or(&rest, "--used-mib", 256u64)?;
            let pre_shrunk = rest.iter().any(|item| item == "--pre-shrunk");
            create_fixture(
                &image,
                sector_size,
                size_mib,
                android_size,
                used_mib,
                pre_shrunk,
            )?;
            print_json(&json!({
                "protocol": 1,
                "event": "fixture-created",
                "helper_version": HELPER_VERSION,
                "image_path": image,
                "logical_sector_size": sector_size,
                "size_mib": size_mib,
                "android_size_mib": android_size,
                "pre_shrunk": pre_shrunk,
                "hardware_writes_enabled": false,
            }))
        }
        "help" | "--help" | "-h" => {
            println!(
                "ipod6g-android-installer {HELPER_VERSION}\n\
                 Phase 0 regular-file-only qualification helper\n\n\
                 Commands:\n\
                   hello\n\
                   dry-run --image PATH [--sector-size 4096] [--android-size-mib 1024] [--output PLAN]\n\
                   commit-image-layout --image PATH --expected-plan-digest SHA256 [--sector-size 4096] [--android-size-mib 1024]\n\
                   verify-image-layout --image PATH [--sector-size 4096] [--android-size-mib 1024]\n\
                   rollback-image-layout --image PATH [--sector-size 4096] [--android-size-mib 1024]\n\
                   create-fixture --image PATH [--size-mib 4096] [--android-size-mib 1024] [--used-mib 256] [--pre-shrunk]\n\n\
                 This build refuses block and character devices. Writes are limited to regular image files."
            );
            Ok(())
        }
        _ => Err(InstallerError::InvalidArgument(format!(
            "unknown command {command}"
        ))),
    }
}

fn print_json<T: Serialize>(value: &T) -> Result<(), InstallerError> {
    println!("{}", serde_json::to_string(value)?);
    Ok(())
}

fn required_value(args: &[String], name: &str) -> Result<String, InstallerError> {
    optional_value(args, name)
        .ok_or_else(|| InstallerError::InvalidArgument(format!("missing required option {name}")))
}

fn optional_value(args: &[String], name: &str) -> Option<String> {
    args.iter()
        .position(|item| item == name)
        .and_then(|index| args.get(index + 1))
        .cloned()
}

fn value_or<T>(args: &[String], name: &str, default: T) -> Result<T, InstallerError>
where
    T: std::str::FromStr,
    T::Err: std::fmt::Display,
{
    match optional_value(args, name) {
        Some(value) => value.parse::<T>().map_err(|error| {
            InstallerError::InvalidArgument(format!("invalid {name} value: {error}"))
        }),
        None => Ok(default),
    }
}

fn error_code(error: &InstallerError) -> &'static str {
    match error {
        InstallerError::InvalidArgument(_) => "invalid-argument",
        InstallerError::UnsafeTarget(_) => "unsafe-target",
        InstallerError::InvalidImage(_) => "invalid-image",
        InstallerError::Io(_) => "io-error",
        InstallerError::Json(_) => "json-error",
    }
}
