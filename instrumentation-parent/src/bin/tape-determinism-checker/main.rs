// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use anyhow::{Context, Result};
use clap::Parser;
use instrumentation_parent::{syscalls::KnownSyscall, FuzzyEq, InstrumentationSpec, TapeEntry};
use serde::{Deserialize, Serialize};
use std::{
    fs::File,
    io::{stdout, BufReader, BufWriter, Write},
    path::PathBuf,
};
use stderrlog::StdErrLog;

#[derive(Debug, Parser)]
struct Args {
    /// Decreases the log level.
    #[clap(
        short,
        long,
        conflicts_with("verbose"),
        max_occurrences(2),
        parse(from_occurrences)
    )]
    quiet: usize,

    /// Increases the log level.
    #[clap(
        short,
        long,
        conflicts_with("quiet"),
        max_occurrences(3),
        parse(from_occurrences)
    )]
    verbose: usize,

    /// The file to output to. If not provided, will output to stdout.
    #[clap(short, long)]
    output_path: Option<PathBuf>,

    /// An input spec that might have been used to generate the tapes.
    #[clap(short, long)]
    spec_path: Option<PathBuf>,

    /// A list of tapes you would like to compare.
    input_tapes: Vec<PathBuf>,
}

fn read_tape_from_file(path: &PathBuf) -> Result<Vec<TapeEntry<KnownSyscall>>> {
    let file = File::open(path).context(format!("When opening file at path {}", path.display()))?;
    let tape = serde_json::from_reader(BufReader::new(file))
        .context(format!("When reading from file at path {}", path.display()))?;
    Ok(tape)
}

type TapePathPair = (PathBuf, Vec<TapeEntry<KnownSyscall>>);

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "snake_case", tag = "type")]
struct DifferencesOutput {
    // A set of disjoint sets of paths,
    // representing the different equivalence classes of tapes
    groups: Vec<Vec<PathBuf>>,
    // each entry with index i,j represents whether there exists a difference between set i, set j
    // and at what index that difference is first discovered.
    // Note that user options will allow us to ignore indices.
    differences: Vec<Vec<Option<usize>>>,
}

// Compares tapes, returns the index where they first differ.
// wrapped in option, or None, if they do not differ.
fn find_tape_differences(
    tape_left: &Vec<TapeEntry<KnownSyscall>>,
    tape_right: &Vec<TapeEntry<KnownSyscall>>,
    ignorable_indices: &Option<Vec<usize>>,
) -> Option<usize> {
    let ignorable_indices = ignorable_indices.as_ref();
    for (i, (elem_left, elem_right)) in tape_left.iter().zip(tape_right.iter()).enumerate() {
        // Check that there is indeed a difference.
        if elem_left.fuzzy_eq(elem_right).is_none()
            // Check membership in ignorable indices set
            && (ignorable_indices
                .and_then(|v| v.iter().find(|&ignore_index| &i == ignore_index)))
            .is_none()
        {
            return Some(i);
        }
    }
    None
}

// Goal is to output Vec<Vec<Option<usize>>
// Where each entry with index i,j represents whether there exists a difference between set i, set j
// We construct this set at the end, after making all of them.
fn make_differences_matrix(
    tape_sets: &Vec<Vec<TapePathPair>>,
    ignorable_indices: &Option<Vec<usize>>,
) -> Vec<Vec<Option<usize>>> {
    let mut differences_set: Vec<Vec<Option<usize>>> =
        vec![vec![None; tape_sets.len()]; tape_sets.len()];
    // Inefficient, for now
    // Could probably memoize these difference computations when
    // we first construct the set
    for (left_index, left_set) in tape_sets.iter().enumerate() {
        for (right_index, right_set) in tape_sets.iter().enumerate().skip(left_index + 1) {
            // Every set should have at least 1 tape
            if (left_set.len() == 0) || (right_set.len() == 0) {
                unreachable!()
            } else {
                let tape_left: &Vec<TapeEntry<KnownSyscall>> = &left_set[0].1;
                let tape_right: &Vec<TapeEntry<KnownSyscall>> = &right_set[0].1;
                let difference = find_tape_differences(tape_left, tape_right, &ignorable_indices);
                // Differences are symmetric here.
                differences_set[left_index][right_index] = difference;
                differences_set[right_index][left_index] = difference;
            }
        }
    }
    differences_set
}

fn add_tape_to_set(
    new_tape_pair: TapePathPair,
    mut tape_sets: Vec<Vec<TapePathPair>>,
) -> Vec<Vec<TapePathPair>> {
    let (new_tape_path, new_tape) = new_tape_pair;
    for tape_set in &mut tape_sets {
        // tape_sets have to be nonempty
        if tape_set.len() == 0 {
            unreachable!()
        } else {
            if tape_set[0].1.fuzzy_eq(&new_tape).is_some() {
                log::debug!(
                    "Adding {} to same set as {}",
                    &new_tape_path.display(),
                    tape_set[0].0.display()
                );
                tape_set.push((new_tape_path, new_tape));
                return tape_sets;
            }
        }
    }
    // Since we haven't returned yet, the new tape
    // hasn't found a set it should be added to,
    // so it's made into its own set
    tape_sets.push(vec![(new_tape_path, new_tape)]);
    tape_sets
}

fn main() -> Result<()> {
    // Get the command-line arguments.
    let args = Args::parse();

    // Set up logging.
    {
        let mut logger = StdErrLog::new();
        match args.quiet {
            0 => logger.verbosity(1 + args.verbose),
            1 => logger.verbosity(0),
            2 => logger.quiet(true),
            // UNREACHABLE: A maximum of two occurrences of quiet are allowed.
            _ => unreachable!(),
        };
        // UNWRAP: No other logger should be set up.
        logger.show_module_names(true).init().unwrap()
    }
    let mut tapes: Vec<(PathBuf, Vec<TapeEntry<KnownSyscall>>)> = Vec::new();
    for path in args.input_tapes {
        log::debug!("Got tape file {:?}", path.display());
        let tape = read_tape_from_file(&path)?;
        tapes.push((path, tape));
    }

    // Get the instrumentation specification from the command line arguments.
    let ignore_index = if let Some(spec_path) = args.spec_path {
        let instrumentation_spec = {
            let file = File::open(spec_path).context("Failed to open instrumentation spec file")?;
            serde_json::from_reader::<_, InstrumentationSpec>(BufReader::new(file))
                .context("Failed to read instrumentation spec file")?
        };
        instrumentation_spec.ignore_indexes
    } else {
        Some(vec![])
    };

    let disjoint_sets = tapes.into_iter().fold(Vec::new(), |acc, next_tape_pair| {
        log::debug!("Adding {:?} to disjoint set", next_tape_pair.0.display());
        add_tape_to_set(next_tape_pair, acc)
    });

    let disjoint_set_differences = make_differences_matrix(&disjoint_sets, &ignore_index);
    let disjoint_sets_paths: Vec<Vec<PathBuf>> = disjoint_sets
        .into_iter()
        .map(|tape_set| tape_set.into_iter().map(|(path, _)| path).collect())
        .collect();
    let output = DifferencesOutput {
        groups: disjoint_sets_paths,
        differences: disjoint_set_differences,
    };

    match args.output_path {
        Some(path) => {
            let mut writer = BufWriter::new(File::create(path)?);
            serde_json::to_writer(&mut writer, &output)?;
            writer.flush()?;
        }
        None => serde_json::to_writer_pretty(stdout(), &output)?,
    };
    // Because we return this after writing,
    // it also functions as a status code saying we were able to write
    // out our results.
    Ok(())
}
