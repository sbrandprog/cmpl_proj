
static const wchar_t * exe_name = L"test.exe";

static ul_hst_t main_hst;
static pla_repo_t main_repo;

static bool main_fill_repo() {
	pla_repo_init(&main_repo, &main_hst);

	main_repo.root = pla_pkg_create(NULL);

	if (!pla_pkg_fill_from_list(main_repo.root, main_repo.hst, L"pla_lib", L"test.pla", NULL)) {
		return false;
	}

	return true;
}

static int main_core() {
	ul_hst_init(&main_hst);

	if (!main_fill_repo()) {
		return false;
	}

	return 0;
}

int main() {
	int res = main_core();

	pla_repo_cleanup(&main_repo);

	ul_hst_cleanup(&main_hst);

	return res;
}
