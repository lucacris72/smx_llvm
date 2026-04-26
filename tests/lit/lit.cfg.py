import os

import lit.formats

config.name = "SMX"
config.test_format = lit.formats.ShTest(True)
config.suffixes = [".c", ".ll", ".s"]
config.test_source_root = os.path.dirname(__file__)

repo_root = os.path.abspath(os.path.join(config.test_source_root, "..", ".."))
config.substitutions.append(("%repo_root", repo_root))

