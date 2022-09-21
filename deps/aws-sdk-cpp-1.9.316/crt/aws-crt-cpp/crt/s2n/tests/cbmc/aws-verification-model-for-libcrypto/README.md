## AWS Verification Model For LibCrypto

Formal verification tools, such as [CBMC](https://github.com/diffblue/cbmc), often require "verification models" of libraries in order to verify code that uses these libraries.
These models provide an abstract implementation of the library API, as opposed to the concrete implementation the library itself provides.
For example, the concrete implementation of a hash-function might invoke a complicated cryptographic routine.
The abstract implementation, on the other hand, might simply assert that the pointers given are valid, and then return an "unconstrained" (AKA "non-deterministic") value.
This allows verification tools to focus on the contracts guaranteed by the API, without having to spend time and memory analyzing the detailed implementation.
It also helps make proofs robust against changes to the library implementation: as long as the library continues to implement the API described in the model, the proof will hold, even if the library changes.

This repository contains a partial verification model for [libCrypto](https://github.com/openssl/openssl).
In particular, it contains abstract models, written in C, of the particular functions used by projects that we have verified.
We do not currently cover all, or even most, of the libCrypto API.
And we do not cover all the functionality of the API that we do cover - in many cases, we model just enough to enable the proofs of other projects to go through.

We welcome contributions modelling the remaining libCrypto functionality.

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This project is licensed under the Apache-2.0 License.

