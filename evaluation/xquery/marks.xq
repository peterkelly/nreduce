import service namespace svc = "http://__HOSTNAME__:__PORT__/marks?WSDL";

<results>{
    let $students := svc:getStudents(),
        $tests := svc:getTests(),
        $maxmarks := sum($tests/marks)
    return
        for $s in $students
        let $source := svc:getSource(string($s/id)),
            $code := svc:compile($source),
            $testres :=
                for $t in $tests
                let $res := svc:runTest($code,string($t/input),string($t/expected))
                return <test marks="{if ($res = 'true') then $t/marks else 0}"/>,
            $total := sum($testres/@marks)
        return
            <student id="{$s/id}" name="{$s/name}">
                {$testres}
                <total>{$total}</total>
                <percent>{100.0 * $total div $maxmarks}</percent>
            </student>
}</results>
