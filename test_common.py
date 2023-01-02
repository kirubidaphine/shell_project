import unittest
import subprocess
import os
import io


_fail_cache = {}

def testFailed (test):
  cached = _fail_cache.get(test, None)
  #if cached is not None: print(test, "cached", cached)
  if cached is not None: return cached
  #print(test, "not cached")

  out = io.StringIO()
  tr = unittest.TextTestRunner(stream=out, failfast=True)
  ts = unittest.defaultTestLoader.loadTestsFromTestCase(test)
  result = tr.run(ts)
  #dir(result)
  def cacheit (v):
    _fail_cache[test] = v
    return v
  if result.errors: return cacheit(True)
  if result.failures: return cacheit(True)
  if result.skipped: return cacheit(True)
  return cacheit(False)

def skipIfFailed (*tests):
  def f (cls):
    for test in tests:
      si = unittest.skipIf(testFailed(test), "Skipped because %s was required to pass" % (test.__name__,))
    return si(cls)
  return f


def _run (cmd, *args):
  # Don't use this!
  want_stderr = False
  if cmd is True:
    want_stderr = True
    cmd = args[0]
    args = args[1:]
  rc,out,err = run_ex(cmd, None, *args)
  if want_stderr:
    return rc,out,err
  return rc,out


def run_ex (cmd, inp, *args, timeout=None):
  if args:
    cmd = [cmd] + [str(s) for s in args]
  else:
    cmd = cmd.split()

  if inp is None:
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       stdin=subprocess.DEVNULL, timeout=timeout)
  else:
    if isinstance(inp, str):
      inp = inp.encode("utf8")
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       input=inp, timeout=timeout)

  return r.returncode,r.stdout.decode("utf8").strip(),r.stderr.decode("utf8").strip()


def lines (s):
  if not s: return []
  return s.split("\n")


class Valgrind (object):
  class Case (unittest.TestCase):
    arg = None
    args = []
    stdin = None
    stdin_newline = True
    timeout = 5

    def setUp (self):
      if self.arg: assert not self.args
      else: assert not self.arg
      if self.arg: arg = self.arg.split(" ")
      else: arg = self.args

      if not os.path.isfile(arg[0]): self.skipTest("Program doesn't exist")

      arg = ["--leak-check=full"] + arg
      stdin = self.stdin
      if isinstance(stdin, str) and self.stdin_newline:
        if not stdin.endswith("\n"): stdin += "\n"
      self.rc,self.r,self.v = run_ex("valgrind", stdin, *arg,
                                     timeout=self.timeout)
      if self.rc < 0: self.skipTest("Program crashed")
      if self.rc == 127: self.skipTest("Valgrind error?")
      #self.assertEqual(self.rc, 0, "Expected 0 return value")
      l = [x[2:] for x in lines(self.v) if x.startswith("==")]
      l = [x.split("==",1)[-1] for x in l]
      self.v = l

    def test_no_definitely_lost (self):
      l = [x for x in self.v if x.strip().startswith("definitely lost: ")]
      if len(l) == 0: return
      assert len(l) == 1
      l = l[0]
      l = int(l.split(":",1)[1].strip().split()[0].replace(",",""))
      self.assertEqual(l, 0, "Some memory definitely lost")

    def test_no_indirectly_lost (self):
      l = [x for x in self.v if x.strip().startswith("indirectly lost: ")]
      if len(l) == 0: return
      assert len(l) == 1
      l = l[0]
      l = int(l.split(":",1)[1].strip().split()[0].replace(",",""))
      self.assertEqual(l, 0, "Some memory definitely lost")

    def test_jump_or_move_on_uninitialized_data (self):
      l = [x for x in self.v if x.strip().startswith("Conditional jump or move depends on uninitialised ")]
      self.assertEqual(len(l), 0, "Conditional jump/move depends on uninitialized data")

    def test_use_of_uninitialized_data (self):
      l = [x for x in self.v if x.strip().startswith("Use of uninitialised value ")]
      self.assertEqual(len(l), 0, "Use of uninitialized data")


class SimpleTest (object):
  class Case (unittest.TestCase):
    prog = None
    args = ()
    stdin = None
    stdin_newline = True # Ensure stdin ends with newline
    expect = False # Fail
    expect_fail = False
    expect_err = ""
    timeout = 5

    def _extraSetUp (self):
      pass

    def setUp (self):
      a = [str(x) for x in self.args]
      if not os.path.isfile(self.prog): self.skipTest("Program doesn't exist")
      self._extraSetUp()
      stdin = self.stdin
      if isinstance(stdin, str) and self.stdin_newline:
        if not stdin.endswith("\n"): stdin += "\n"
      self.rc,self.r,self.e = run_ex(self.prog, stdin, *a, timeout=self.timeout)

    def test (self):
      if self.rc < 0: self.fail("Program crashed!")
      if self.expect_fail:
        self.assertNotEqual(self.rc, 0, "Expected nonzero return value")
        self._check_output()
      else:
        self.assertEqual(self.rc, 0, "Expected zero return value")
        self._check_output()

    def _check_output (self):
      if self.expect is not False:
        self.assertEqual(self.r, self.expect)
      if self.expect_err is not False:
        self.assertEqual(self.e, self.expect_err)
