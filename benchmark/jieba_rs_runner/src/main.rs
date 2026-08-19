use jieba_rs::Jieba;
use std::env;
use std::fs;
use std::time::Instant;

struct Options {
    corpus: String,
    mode: String,
    iterations: usize,
    warmup: usize,
    dump_tokens: bool,
}

fn parse_options() -> Options {
    let mut args = env::args().skip(1);
    let mut options = Options { corpus: String::new(), mode: "exact".into(), iterations: 1, warmup: 1, dump_tokens: false };
    while let Some(arg) = args.next() {
        let mut value = || args.next().expect("missing option value");
        match arg.as_str() {
            "--corpus" => options.corpus = value(),
            "--mode" => options.mode = value(),
            "--iterations" => options.iterations = value().parse().unwrap(),
            "--warmup" => options.warmup = value().parse().unwrap(),
            "--dump-tokens" => options.dump_tokens = true,
            _ => panic!("unknown option: {arg}"),
        }
    }
    assert!(!options.corpus.is_empty(), "--corpus is required");
    assert!(options.mode == "exact" || options.mode == "search");
    options
}

fn consume_token(checksum: &mut u64, token: &str) {
    *checksum ^= (token.len() as u64)
        .wrapping_add(0x9e3779b97f4a7c15)
        .wrapping_add(*checksum << 6)
        .wrapping_add(*checksum >> 2);
    if let (Some(first), Some(last)) = (token.as_bytes().first(), token.as_bytes().last()) {
        *checksum ^= u64::from(*first);
        *checksum = checksum.rotate_left(13);
        *checksum ^= u64::from(*last);
    }
}

fn main() {
    let options = parse_options();
    let storage = fs::read_to_string(&options.corpus).expect("cannot read corpus");
    let documents: Vec<&str> = storage.lines().filter(|line| !line.is_empty()).collect();
    let input_bytes: usize = documents.iter().map(|document| document.len()).sum();
    let dict_file = fs::File::open("benchmark/third_party/cppjieba/dict/jieba.dict.utf8").expect("cannot open shared dictionary");
    let mut dict_reader = std::io::BufReader::new(dict_file);
    let jieba = Jieba::with_dict(&mut dict_reader).expect("cannot load shared dictionary");

    let run = |iterations: usize, dump_tokens: bool| {
        let mut checksum = 0u64;
        let mut token_count = 0usize;
        for _ in 0..iterations {
            for document in &documents {
                let tokens = if options.mode == "exact" {
                    jieba.cut(document, true)
                } else {
                    jieba.cut_for_search(document, true)
                };
                token_count += tokens.len();
                for token in tokens {
                    consume_token(&mut checksum, token.word);
                    if dump_tokens {
                        print!("{}\t", token.word);
                    }
                }
                if dump_tokens {
                    println!();
                }
            }
        }
        (token_count, checksum)
    };

    if options.dump_tokens {
        run(1, true);
        return;
    }
    run(options.warmup, false);
    let start = Instant::now();
    let (token_count, checksum) = run(options.iterations, false);
    let elapsed = start.elapsed().as_nanos();
    println!("implementation,mode,input_bytes,documents,iterations,tokens,checksum,elapsed_ns");
    println!("jieba-rs,{},{},{},{},{},{},{}", options.mode, input_bytes, documents.len(), options.iterations, token_count, checksum, elapsed);
}
